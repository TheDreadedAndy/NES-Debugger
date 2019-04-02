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
#include "memory.h"

// Helper functions.
regfile_t *regfile_new(void);

int main(int argc, char *argv[]) {

  // Global variables needed for getopt.
  extern char *optarg;
  extern int optind, opterr, optopt;

  // Parses the users command line input.
  size_t iterations = 0;
  char *file = NULL;
  char opt;

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

  // Prepares the 2A03 for execution.
  memory_t *M = memory_new(file);
  regfile_t *R = regfile_new();
  state_t *S = state_new();
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);

  for (size_t i = 0; i < iterations; i++) {
    // Executes the next cycle and prints the results.
    cpu_run_cycle(R, M, S);
    printf("State following iteration %d:\n", i);
    printf("A: %x, X: %x, Y: %x, INST: %x\n", R->A, R->X, R->Y, R->inst);
    printf("State (flags): %x, Stack pointer: %x\n", R->P, R->S);
    printf("PCL: %x, PCH: %x\n", R->PCL, R->PCH);
    printf("Abstraction register state:\n");
    printf("MDR: %x, Carry: %x\n", R->MDR, R->carry);
    printf("Addr Low: %x, Addr High: %x\n", R->addrL, R->addrH);
    printf("Pointer Low: %x, Pointer High: %x\n", R->ptrL, R->ptrH);
  }

  return 0;
}

regfile_t *regfile_new(void) {
  regfile_t *R = xcalloc(1, sizeof(regfile_t));
  R->P = 0x20; // TODO: verify this.
  return R;
}
