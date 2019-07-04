// The current idea is for ndb to be a complete reimplementation
// of gdb for the NES. Using NesIGuess as its emulator.
//
// This will be the main file (at least that's the plan,
// I may split it up more).

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "./util/util.h"
#include "./cpu/2A03.h"
#include "./cpu/regs.h"
#include "./memory/memory.h"
#include "./memory/header.h"
#include "./ppu/ppu.h"

/* Helper functions */
void start_emulation(char *file);

/*
 * Loads in the users arguments and starts ndb.
 */
int main(int argc, char *argv[]) {

  // Global variables needed for getopt.
  extern char *optarg;
  extern int optind, opterr, optopt;

  // Parses the users command line input.
  size_t iterations = 0;
  char *file = NULL;
  signed char opt;
  bool verbose = false;

  while ((opt = getopt(argc, argv, "hvi:f:")) != -1) {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'i':
        iterations = atoi(optarg);
        break;
      case 'f':
        file = optarg;
        break;
      default:
        printf("usage: ndb -i <NUM> -f <FILE>\n");
        exit(0);
        break;
    }
  }

  // Ensures that the user specified an NES file.
  if (file == NULL) {
    printf("usage: ndb -i <NUM> -f <FILE>\n");
    exit(0);
  }

  // Prepares the NES emulation for execution.
  start_emulation(file);

  printf("Starting emulation...\n");
  for (size_t i = 0; i < iterations; i++) {
    // Executes the next cycle and prints the results.
    cpu_run_cycle();
    if (verbose) { regfile_print(i); }
  }
  printf("Done!\n");

  // Clean up any allocated memory.
  cpu_free();
  ppu_free();

  return 0;
}

/*
 * Takes in a file location for an NES rom file, and uses it to prepare
 * the emulation.
 *
 * Assumes the file location is valid.
 */
void start_emulation(char *file) {
  // Open the rom.
  FILE *rom_file = fopen(file, "r");

  // Decode the header so that the emulation can be prepared.
  header_t *header = decode_header(rom_file);
  if (header == NULL) { exit(-1); }

  // Initializes the hardware emulation.
  cpu_init(rom_file, header);
  ppu_init();

  // Clean up and exit.
  fclose(rom_file);
  return;
}
