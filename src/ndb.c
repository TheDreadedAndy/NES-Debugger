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
#include "util.h"
#include "2A03.h"
#include "cpu_regs.h"
#include "memory.h"

// Loads in the users argument and starts ndb.
int main(int argc, char *argv[]) {

  // Global variables needed for getopt.
  extern char *optarg;
  extern int optind, opterr, optopt;

  // Parses the users command line input.
  size_t iterations = 0;
  char *file = NULL;
  signed char opt;

  while ((opt = getopt(argc, argv, "hi:f:")) != -1) {
    switch (opt) {
      case 'h':
        printf("Hey man, don't we all need help?\n");
        break;
      case 'i':
        iterations = atoi(optarg);
        break;
      case 'f':
        file = optarg;
        break;
      default:
        printf("usage: ndb -i <NUM> -f <FILE>\n");
        exit(1);
        break;
    }
  }

  if (file == NULL) {
    printf("Please specify a file\n");
    exit(0);
  }

  // Prepares the 2A03 for execution.
  memory_t *M = memory_new(file);
  regfile_t *R = regfile_new(M);
  state_t *S = state_new();
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);

  for (int i = 0; i < iterations; i++) {
    // Executes the next cycle and prints the results.
    cpu_run_cycle(R, M, S);
    regfile_print(R, i);
  }

  free(R);
  //memory_free(M);
  state_free(S);

  return 0;
}
