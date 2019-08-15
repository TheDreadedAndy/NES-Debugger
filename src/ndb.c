// The current idea is for ndb to be a complete reimplementation
// of gdb for the NES. Using NesIGuess as its emulator.
//
// This will be the main file (at least that's the plan,
// I may split it up more).

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include "./ndb.h"
#include "./emulation/emutime.h"
#include "./sdl/window.h"
#include "./sdl/render.h"
#include "./sdl/input.h"
#include "./util/util.h"
#include "./cpu/2A03.h"
#include "./cpu/regs.h"
#include "./memory/memory.h"
#include "./memory/header.h"
#include "./ppu/ppu.h"
#include "./apu/apu.h"

// The number of APU cycles that will be emulated per emulation cycle.
// Setting this too low will cause speed issues.
// Setting this too high will cause timing issues.
#define EMU_CYCLE_SIZE 4972

// Global running variable. Available to other files through ndb.h.
// Setting this value to false closes the program.
bool ndb_running = true;

/* Helper functions */
void start_emulation(char *rom, char *pal);
clock_t get_delay(clock_t last_time);
void run_emulation_cycle(void);

/*
 * Loads in the users arguments and starts ndb.
 */
int main(int argc, char *argv[]) {

  // Global variables needed for getopt.
  extern char *optarg;
  extern int optind, opterr, optopt;

  // Parses the users command line input.
  char *rom_file = NULL;
  char *pal_file = NULL;
  signed char opt;

  while ((opt = getopt(argc, argv, "hf:p:")) != -1) {
    switch (opt) {
      case 'f':
        rom_file = optarg;
        break;
      case 'p':
        pal_file = optarg;
        break;
      default:
        printf("usage: ndb -f <FILE> -p <PALETTE FILE>\n");
        exit(0);
        break;
    }
  }

  // Ensures that the user specified an NES file.
  if (rom_file == NULL || pal_file == NULL) {
    printf("usage: ndb -f <ROM FILE> -p <PALETTE FILE>\n");
    exit(0);
  }

  // Prepares the NES emulation for execution.
  window_init();
  input_load(NULL);
  start_emulation(rom_file, pal_file);

  // Init the timing variables.
  emutime_t current_time, sdl_wait, emu_wait, fps_wait;
  emutime_get(&sdl_wait);
  emutime_get(&emu_wait);
  emutime_get(&fps_wait);
  time_t last_fps_display = time(NULL);
  double frames_drawn = 0;

  // Main emulation loop.
  while (ndb_running) {
    // Get the clock time for this loop.
    emutime_get(&current_time);

    // Process any events on the SDL event queue.
    if (emutime_gt(&current_time, &sdl_wait)) {
      window_process_events();
      emutime_update(&current_time, &sdl_wait, FRAME_LENGTH);
    }

    // Update the frame rate display.
    if (emutime_gt(&current_time, &fps_wait)) {
      time_t now = time(NULL);
      window_display_fps(frames_drawn / difftime(now, last_fps_display));
      last_fps_display = now;
      frames_drawn = 0;
      emutime_update(&current_time, &fps_wait, NSECS_PER_SEC);
    }

    // Update the timing for the emulation.
    if (render_has_drawn()) {
      emutime_update(&current_time, &emu_wait, FRAME_LENGTH);
      frames_drawn++;
    }

    // Executes the next cycle.
    if (emutime_gt(&current_time, &emu_wait)) { run_emulation_cycle(); }
  }

  // Clean up any allocated memory.
  cpu_free();
  ppu_free();
  apu_free();
  window_close();

  return 0;
}

/*
 * Takes in a file location for an NES rom file, and uses it to prepare
 * the emulation.
 *
 * Assumes the file location is valid.
 */
void start_emulation(char *rom, char *pal) {
  // Open the rom.
  FILE *rom_file = fopen(rom, "rb");

  // Decode the header so that the emulation can be prepared.
  header_t *header = decode_header(rom_file);
  if (header == NULL) { exit(-1); }

  // Initializes the hardware emulation.
  cpu_init(rom_file, header);
  ppu_init(pal);
  apu_init();

  // Clean up and exit.
  fclose(rom_file);
  return;
}

/*
 * Runs a predetermined number of emulation cycles.
 *
 * Assumes the emulation has been initialized.
 */
void run_emulation_cycle(void) {
  for (size_t i = 0; i < EMU_CYCLE_SIZE; i++) {
    // The PPU is clocked at 3x the rate of the CPU, the APU is clocked
    // at 1/2 the rate of the CPU.
    ppu_run_cycle();
    ppu_run_cycle();
    ppu_run_cycle();
    cpu_run_cycle();
    ppu_run_cycle();
    ppu_run_cycle();
    ppu_run_cycle();
    cpu_run_cycle();
    apu_run_cycle();
  }
  return;
}
