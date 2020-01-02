/*
 * TODO
 */

#include "./emulation.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "../config/config.h"
#include "../sdl/window.h"
#include "../sdl/renderer.h"
#include "../sdl/audio_player.h"
#include "../sdl/input.h"
#include "../memory/memory.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../util/contracts.h"
#include "./signals.h"

// The frame rate of the NES.
#define NES_FRAME_RATE 60L

// The number of nanoseconds in a second.
#define NSECS_PER_SEC (1000000000L)

// The number of CPU cycles that will be emulated per emulation cycle.
// With the current timing system, this must be set to one 60th of the
// 2A03 clock rate.
#define EMU_CYCLE_SIZE 29830

/*
 * Attempts to create an emulation object using the given configuration object
 * and rom file.
 *
 * Returns NULL on failure. Fails if any of the objects necessary for the
 * emulation cannot be created.
 */
Emulation *Emulation::Create(FILE *rom, Config *config) {
  // Attempt to create the SDL window.
  Window *window = Window::Create(config);
  if (window == NULL) {
    fprintf(stderr, "Error: failed to create SDL window.\n");
    return NULL;
  }

  // Attempt to create a Memory object with the given rom file.
  Memory *memory = Memory::Create(rom);
  if (memory == NULL) {
    fprintf(stderr, "Error: Failed to create emulated memory for rom.\n");
    delete window;
    return NULL;
  }

  // Create the NES APU, CPU, and PPU.
  Cpu *cpu = new Cpu();
  Ppu *ppu = new Ppu(config);
  Apu *apu = new Apu();

  // Connect all of the emulated NES systems together.
  memory->AddController(window->GetInput());
  memory->Connect(cpu, ppu, apu);
  cpu->Connect(memory);
  ppu->Connect(memory, window->GetRenderer(), &(cpu->nmi_line_));
  apu->Connect(memory, window->GetAudioPlayer(), &(cpu->irq_line_));

  // Prepare the CPU for the emulation.
  cpu->Power();

  // Create and return an emulation object.
  return new Emulation(window, memory, cpu, ppu, apu);
}

/*
 * Uses the provided objects to create an emulation object.
 */
Emulation::Emulation(Window *window, Memory *memory,
                     Cpu *cpu, Ppu *ppu, Apu *apu) {
  window_ = window;
  memory_ = memory;
  cpu_ = cpu;
  ppu_ = ppu;
  apu_ = apu;
  return;
}

/*
 * Runs the main emulation loop.
 * Returns when a termination signal is received.
 *
 * Assumes signals have been initialized.
 */
void Emulation::Run(void) {
  while (ndb_running) {
    // Syncs the emulation to 60 FPS, when possible.
    SyncFrameRate();

    // Updates the frame rate display.
    UpdateFrameCounter();

    // Processes any events on the SDL queue.
    window_->ProcessEvents();

    // Executes the next frame of emulation.
    RunEmulationCycle();
  }
  return;
}

/*
 * Ensures that the program waits at least 1/60 seconds between calls
 * to this function. Used to time emulation.
 */
void Emulation::SyncFrameRate(void) {
  // Determine the minimum time at which this function can return.
  EmuTime wait_spec = { 0, NSECS_PER_SEC / NES_FRAME_RATE };
  wait_spec.tv_sec = last_sync_time_.tv_sec;
  wait_spec.tv_nsec += last_sync_time_.tv_nsec;
  if (wait_spec.tv_nsec >= NSECS_PER_SEC) {
    wait_spec.tv_nsec -= NSECS_PER_SEC;
    wait_spec.tv_sec++;
  }

  // Pause the program until the minimum time has been reached.
  EmuTime current_time, diff;
  TimeGet(&current_time);
  if (TimeGt(&wait_spec, &current_time)) {
    TimeDiff(&wait_spec, &current_time, &diff);
    nanosleep(&diff, NULL);
  }

  // Update the last call time.
  TimeGet(&last_sync_time_);

  return;
}

/*
 * Updates the SDL window with the current running frame rate every
 * 60 frames.
 */
void Emulation::UpdateFrameCounter(void) {
  // Counts how many frames have been run.
  frames_counted_++;

  // Determine if it is time to update the frame rate display.
  EmuTime current_time, diff;
  if (frames_counted_ >= NES_FRAME_RATE) {
    // Update the frame rate display.
    TimeGet(&current_time);
    TimeDiff(&current_time, &last_frame_time_, &diff);
    float secs_passed = (static_cast<float>(diff.tv_sec))
                      + ((static_cast<float>(diff.tv_nsec))
                      / (static_cast<float>(NSECS_PER_SEC)));
    window_->DisplayFps((static_cast<float>(frames_counted_)) / secs_passed);
    frames_counted_ = 0;

    // Update the last called time.
    TimeGet(&last_frame_time_);
  }

  return;
}

/*
 * Gets the current time and stores it in a time spec structure.
 */
void Emulation::TimeGet(EmuTime *time) {
#ifdef _NES_OSLIN
  clock_gettime(CLOCK_MONOTONIC_RAW, time);
#else
  clock_gettime(CLOCK_MONOTONIC, time);
#endif
  return;
}

/*
 * Checks if the first time is greater than the second time.
 *
 * Assumes the times are non-null.
 */
bool Emulation::TimeGt(EmuTime *time1, EmuTime *time2) {
  return (time1->tv_sec > time2->tv_sec) || ((time1->tv_sec == time2->tv_sec)
                                         && (time1->tv_nsec > time2->tv_nsec));
}

/*
 * Subtracts the first time from the second time, storing the result in res.
 *
 * Assumes the first time is greater than the second time.
 */
void Emulation::TimeDiff(EmuTime *time1, EmuTime *time2, EmuTime *res) {
  CONTRACT(TimeGt(time1, time2));

  // Borrow from the seconds in nanoseconds, if needed.
  if (time1->tv_nsec < time2->tv_nsec) {
    time1->tv_nsec += NSECS_PER_SEC;
    time1->tv_sec--;
  }

  // Calculate the difference.
  res->tv_sec = time1->tv_sec - time2->tv_sec;
  res->tv_nsec = time1->tv_nsec - time2->tv_nsec;

  return;
}

/*
 * Runs the NES emulation for 1/60th of its clock rate.
 */
void Emulation::RunEmulationCycle(void) {
  for (size_t i = 0; i < EMU_CYCLE_SIZE; i++) {
    // The PPU is clocked at 3x the rate of the CPU.
    ppu_->RunCycle();
    ppu_->RunCycle();
    ppu_->RunCycle();
    cpu_->RunCycle();
    apu_->RunCycle();
  }
  return;
}

/*
 * Deletes the calling Emulation object.
 */
Emulation::~Emulation(void) {
  delete apu_;
  delete ppu_;
  delete cpu_;
  delete memory_;
  delete window_;
  return;
}
