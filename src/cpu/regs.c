/*
 * This file contains tools for creating and interracting with
 * the 2A03's regfile. Note that it is still intended for the
 * regfile structure to be used directly, this file just contains
 * general helper functions.
 */

#include <stdlib.h>
#include <stdio.h>
#include "../util/util.h"
#include "../util/data.h"
#include "../memory/memory.h"
#include "./regs.h"

/* Global register file. Used in CPU emulation */
regfile_t *R = NULL;

/*
 * Initializes the global register file.
 *
 * Assumes the register file has not already been initialized.
 */
void regfile_init(void) {
  R = xcalloc(1, sizeof(regfile_t));
  // IRQ's are disabled on power up.
  R->P = 0x04U;
  return;
}

/*
 * Increments the PC registers in a regfile.
 * Assumes the regfile is non-null.
 */
void regfile_inc_pc(void) {
  R->pc_lo++;
  if (R->pc_lo == 0) { R->pc_hi++; }
  return;
}

/*
 * Prints a regfile.
 */
void regfile_print(size_t i) {
  printf("-----------------------------------\n");
  printf("State following iteration %d:\n", (int)i);
  printf("A: %x, X: %x, Y: %x, INST: %x\n", R->A,
         R->X, R->Y, R->inst);
  printf("State (flags): %x, Stack pointer: %x\n", R->P,
         R->S);
  printf("PCL: %x, PCH: %x\n", R->pc_lo, R->pc_hi);
  printf("Abstraction register state:\n");
  printf("MDR: %x, Carry: %x\n", R->mdr, R->carry);
  printf("Addr Low: %x, Addr High: %x\n", R->addr_lo,
         R->addr_hi);
  printf("Pointer Low: %x, Pointer High: %x\n", R->ptr_lo,
         R->ptr_hi);
  return;
}
