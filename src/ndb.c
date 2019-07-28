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
#include "./sdl/window.h"
#include "./sdl/render.h"
#include "./util/util.h"
#include "./cpu/2A03.h"
#include "./cpu/regs.h"
#include "./memory/memory.h"
#include "./memory/header.h"
#include "./ppu/ppu.h"

// The number of clocks per frame of emulation.
// Used to throttle the emulation.
#define FRAME_LENGTH (CLOCKS_PER_SEC / 60)

// Global running variable. Available to other files through ndb.h.
// Setting this value to false closes the program.
bool ndb_running = true;

/* Helper functions */
void start_emulation(char *rom, char *pal);
clock_t get_delay(clock_t last_time);

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
  bool verbose = false;

  while ((opt = getopt(argc, argv, "hvf:p:")) != -1) {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'f':
        rom_file = optarg;
        break;
      case 'p':
        pal_file = optarg;
        break;
      default:
        printf("usage: ndb -f <FILE>\n");
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
  start_emulation(rom_file, pal_file);

  // Init the timing variables.
  clock_t sdl_wait = clock();
  clock_t emu_wait = sdl_wait;

  // Main emulation loop.
  printf("Starting emulation...\n");
  while (ndb_running) {
    // Process any events on the SDL event queue.
    if (clock() > sdl_wait) {
      window_process_events();
      sdl_wait = get_delay(sdl_wait);
    }

    // Update the timing for the emulation.
    if (render_has_drawn()) { emu_wait = get_delay(emu_wait); }

    // Executes the next cycle and prints the results.
    if (clock() > emu_wait) {
      ppu_run_cycle();
      ppu_run_cycle();
      ppu_run_cycle();
      cpu_run_cycle();
      if (verbose) { regfile_print(0); }
    }
  }
  printf("Done!\n");

  // Clean up any allocated memory.
  cpu_free();
  ppu_free();
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
  FILE *rom_file = fopen(rom, "r");

  // Decode the header so that the emulation can be prepared.
  header_t *header = decode_header(rom_file);
  if (header == NULL) { exit(-1); }

  // Initializes the hardware emulation.
  cpu_init(rom_file, header);
  ppu_init(pal);

  // Clean up and exit.
  fclose(rom_file);
  return;
}

/*
 * Determines the next clock time an action should be performed using the time
 * at which it was started and the current time, assuming the action should run
 * 60 times a second.
 *
 * The returned value is the time at which the action should again be performed.
 */
clock_t get_delay(clock_t last_time) {
  // Check if its been longer then a frame since the last delay.
  clock_t current_time = clock();
  if (FRAME_LENGTH < (current_time - last_time)) { return current_time; }

  // Otherwise, set a delay to help the emulator run at 60FPS.
  return last_time + FRAME_LENGTH;
}
