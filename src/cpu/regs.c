/*
 * This file contains tools for creating and interracting with
 * the 2A03's regfile. Note that it is still intended for the
 * regfile structure to be used directly, this file just contains
 * general helper functions.
 */

#include <stdlib.h>
#include <stdio.h>
#include "../util/util.h"
#include "../memory/memory.h"
#include "./regs.h"

/*
 * Creates a regfile and inits it.
 */
regfile_t *regfile_new(memory_t *M) {
  regfile_t *R = xcalloc(1, sizeof(regfile_t));

  // We need to read in the initial memory location from memory.
  R->pc_lo = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH, M);
  R->pc_hi = memory_read(MEMORY_RESET_LOW+1, MEMORY_RESET_HIGH, M);
  return R;
}

/*
 * Increments the PC registers in a regfile.
 * Assumes the regfile is non-null.
 */
void regfile_inc_pc(regfile_t *R) {
  dword_t pc = (((dword_t)(R->pc_hi)) << 8) | R->pc_lo;
  pc++;
  R->pc_lo = (word_t)pc;
  R->pc_hi = (word_t)(pc >> 8);
  return;
}

/*
 * Prints a regfile.
 */
void regfile_print(regfile_t *R, size_t i) {
  printf("-----------------------------------\n");
  printf("State following iteration %d:\n", (int)i);
  printf("A: %x, X: %x, Y: %x, INST: %x\n", R->A, R->X, R->Y, R->inst);
  printf("State (flags): %x, Stack pointer: %x\n", R->P, R->S);
  printf("PCL: %x, PCH: %x\n", R->pc_lo, R->pc_hi);
  printf("Abstraction register state:\n");
  printf("MDR: %x, Carry: %x\n", R->mdr, R->carry);
  printf("Addr Low: %x, Addr High: %x\n", R->addr_lo, R->addr_hi);
  printf("Pointer Low: %x, Pointer High: %x\n", R->ptr_lo, R->ptr_hi);
  return;
}
