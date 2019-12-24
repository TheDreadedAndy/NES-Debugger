/*
 * TODO
 */

#include "./ndb.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <getopt.h>

#include "./emulation/emutime.h"
#include "./sdl/window.h"
#include "./sdl/renderer.h"
#include "./sdl/audio_player.h"
#include "./sdl/input.h"
#include "./util/util.h"
#include "./cpu/cpu.h"
#include "./memory/memory.h"
#include "./memory/header.h"
#include "./ppu/ppu.h"
#include "./apu/apu.h"

// The number of cpu cycles that will be emulated per emulation cycle.
// With the current timing system, this must be set to one 60th of the
// 2A03 clock rate.
#define EMU_CYCLE_SIZE 29830

// Global running variable. Available to other files through ndb.h.
// Setting this value to false closes the program.
bool ndb_running = true;

/* Helper functions */
static void RunEmulationCycle(Cpu *cpu, Ppu *ppu, Apu *apu);

/*
 * Loads in the users arguments and starts ndb.
 */
int main(int argc, char *argv[]) {

  // Global variables needed for getopt.
  extern char *optarg;

  // Long option array, used to parse input.
  struct option long_opts[] = {
    { "surface", 0, NULL, 's' },
    { "hardware", 0, NULL, 'h' },
    { "file", 1, NULL, 'f' },
    { "palette", 1, NULL, 'p' }
  };

  // Parses the users command line input.
  char *rom_file = NULL;
  Config *config = new Config(NULL);
  signed char opt;

  while ((opt = getopt_long(argc, argv, "hf:p:s", long_opts, NULL)) != -1) {
    switch (opt) {
      case 'f':
        rom_file = optarg;
        break;
      case 'p':
        config->Set(kPaletteFileKey, optarg);
        break;
      case 's':
        config->Set(kRendererTypeKey, kRendererSurfaceVal);
        break;
      case 'v':
        config->Set(kRendererTypeKey, kRendererHardwareVal);
        break;
      default:
        printf("usage: ndb -f <FILE> -p <PALETTE FILE>\n");
        delete config;
        exit(0);
        break;
    }
  }

  // Create the SDL window used by the emulation.
  Window *window = Window::Create(config);

  // Open the rom. Prompt the user to select one if they did not already provide
  // one.
  FILE *rom = NULL;
  if (rom_file != NULL) {
    rom = fopen(rom_file, "rb");
  } else {
    OpenFile(&rom);
  }

  // Verify that the users file was opened correctly.
  if (rom == NULL) {
    fprintf(stderr, "Failed to open the specified file.\n");
    abort();
  }

  // Use the rom file to create the memory object.
  Memory *memory = Memory::Create(rom);
  memory->AddController(window->GetInput());

  // Use the memory object to create the NES CPU, PPU, and APU.
  Cpu *cpu = new Cpu(memory);
  Ppu *ppu = new Ppu(memory, window->GetRenderer(), &(cpu->nmi_line_), config);
  Apu *apu = new Apu(window->GetAudioPlayer(), memory, &(cpu->irq_line_));

  // Connect the CPU, PPU, and APU to memory.
  memory->Connect(cpu, ppu, apu);

  // Close the rom file.
  fclose(rom);

  // Main emulation loop.
  while (ndb_running) {
    // Syncs the emulation to 60 FPS, when possible.
    EmutimeSyncFrameRate(NES_FRAME_RATE);

    // Update the frame rate display.
    EmutimeUpdateFrameCounter(NES_FRAME_RATE, window);

    // Process any events on the SDL queue.
    window->ProcessEvents();

    // Execute the next frame of emulation.
    RunEmulationCycle(cpu, ppu, apu);
  }

  // Saves any changes the user made to the config.
  config->Save();

  // Clean up any allocated memory.
  delete apu;
  delete ppu;
  delete cpu;
  delete memory;
  delete window;
  delete config;

  return 0;
}

/*
 * Runs a predetermined number of emulation cycles.
 *
 * Assumes the emulation has been initialized.
 */
static void RunEmulationCycle(Cpu *cpu, Ppu *ppu, Apu *apu) {
  for (size_t i = 0; i < EMU_CYCLE_SIZE; i++) {
    // The PPU is clocked at 3x the rate of the CPU, the APU is clocked
    // at 1/2 the rate of the CPU.
    ppu->RunCycle();
    ppu->RunCycle();
    ppu->RunCycle();
    cpu->RunCycle();
    apu->RunCycle();
  }
  return;
}
