// TODO: make this file.
//
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

// Loads in the users argument and starts ndb.
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

  if (file == NULL) {
    printf("usage: ndb -i <NUM> -f <FILE>\n");
    exit(0);
  }

  // Prepares the 2A03 for execution.
  memory_t *M = memory_new(file);
  regfile_t *R = regfile_new(M);
  state_t *S = state_new();
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);

  printf("Starting emulation...\n");
  for (size_t i = 0; i < iterations; i++) {
    // Executes the next cycle and prints the results.
    cpu_run_cycle(R, M, S);
    if (verbose) { regfile_print(R, i); }
  }
  printf("Done!\n");

  free(R);
  memory_free(M);
  state_free(S);

  return 0;
}

/*
 * Takes in a file location for an NES rom file, and uses it to prepare
 * the emulation. Assumes the file location is valid.
 */
void start_emu(char *file) {
  // Open the rom and get its size.
  FILE *rom_file = fopen(file, "r");
  size_t rom_size = 0;
  while (fgetc(rom_file) != EOF) { rom_size++; }

  // Decode the header so that the emulation can be prepared.
  header_t *header = decode_header(rom_file, rom_size);

  return;
}
