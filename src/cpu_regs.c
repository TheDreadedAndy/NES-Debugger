/*
 * This file contains tools for creating and interracting with
 * the 2A03's regfile. Note that it is still intended for the
 * regfile structure to be used directly, this file just contains
 * general helper functions.
 */

#include <stdlib.h>
#include <stdio.h>
#include "util.h"
#include "memory.h"
#include "cpu_regs.h"

// Creates a regfile and inits it.
regfile_t *regfile_new(memory_t *M) {
  regfile_t *R = xcalloc(1, sizeof(regfile_t));
  R->P = 0x20; // TODO: verify this.
  R->PCL = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH, M);
  R->PCH = memory_read(MEMORY_RESET_LOW+1, MEMORY_RESET_HIGH, M);
  return R;
}

// Prints a regfile.
void regfile_print(regfile_t *R, size_t i) {
  printf("-----------------------------------\n");
  printf("State following iteration %d:\n", (int)i);
  printf("A: %x, X: %x, Y: %x, INST: %x\n", R->A, R->X, R->Y, R->inst);
  printf("State (flags): %x, Stack pointer: %x\n", R->P, R->S);
  printf("PCL: %x, PCH: %x\n", R->PCL, R->PCH);
  printf("Abstraction register state:\n");
  printf("MDR: %x, Carry: %x\n", R->MDR, R->carry);
  printf("Addr Low: %x, Addr High: %x\n", R->addrL, R->addrH);
  printf("Pointer Low: %x, Pointer High: %x\n", R->ptrL, R->ptrH);
  return;
}
