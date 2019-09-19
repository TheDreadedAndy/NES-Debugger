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
#include "./sdl/audio.h"
#include "./sdl/input.h"
#include "./util/util.h"
#include "./cpu/2A03.h"
#include "./cpu/regs.h"
#include "./memory/memory.h"
#include "./memory/header.h"
#include "./ppu/ppu.h"
#include "./apu/apu.h"

// The number of cpu cycles that will be emulated per emulation cycle.
// Setting this too low will cause speed issues.
// Setting this too high will cause timing issues.
#define EMU_CYCLE_SIZE 29830

// Global running variable. Available to other files through ndb.h.
// Setting this value to false closes the program.
bool ndb_running = true;

/* Helper functions */
static void start_emulation(char *rom, char *pal);
static void run_emulation_cycle(void);

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

  while ((opt = getopt(argc, argv, "hf:p:t")) != -1) {
    switch (opt) {
      case 'f':
        rom_file = optarg;
        break;
      case 'p':
        pal_file = optarg;
        break;
      case 't':
        enable_high_freqs = true;
        break;
      default:
        printf("usage: ndb -f <FILE> -p <PALETTE FILE>\n");
        exit(0);
        break;
    }
  }

  // Prepares the NES emulation for execution.
  window_init();
  audio_init();
  input_load(NULL);
  start_emulation(rom_file, pal_file);

  // Main emulation loop.
  while (ndb_running) {
    // Syncs the emulation to 60 FPS, when possible.
    emutime_sync_frame_rate(NES_FRAME_RATE);

    // Update the frame rate display.
    emutime_update_frame_counter(NES_FRAME_RATE);

    // Process any events on the SDL queue.
    window_process_events();

    // Execute the next frame of emulation.
    run_emulation_cycle();
  }

  // Clean up any allocated memory.
  cpu_free();
  ppu_free();
  apu_free();
  audio_close();
  window_close();

  return 0;
}

/*
 * Takes in a file location for an NES rom file, and uses it to prepare
 * the emulation.
 *
 * Assumes the file location is valid.
 */
static void start_emulation(char *rom, char *pal) {
  // Open the rom. Prompt the user to select one if they did not already provide
  // one.
  FILE *rom_file = NULL;
  if (rom != NULL) {
    rom_file = fopen(rom, "rb");
  } else {
    open_file(&rom_file);
  }

  // Verify that the users file was opened correctly.
  if (rom_file == NULL) {
    fprintf(stderr, "Failed to open the specified file.\n");
    abort();
  }

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
static void run_emulation_cycle(void) {
  for (size_t i = 0; i < EMU_CYCLE_SIZE; i++) {
    // The PPU is clocked at 3x the rate of the CPU, the APU is clocked
    // at 1/2 the rate of the CPU.
    ppu_run_cycle();
    ppu_run_cycle();
    ppu_run_cycle();
    cpu_run_cycle();
    apu_run_cycle();
  }
  return;
}
