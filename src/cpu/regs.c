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
  // The stack pointer always has the same high address byte.
  R->S.w[WORD_HI] = MEMORY_STACK_HIGH;
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
         R->S.w[WORD_LO]);
  printf("PC: %x\n", R->pc.dw);
  printf("Abstraction register state:\n");
  printf("MDR: %x, Carry: %x\n", R->mdr, R->carry);
  printf("Addr %x\n", R->addr.dw);
  printf("Pointer: %x\n", R->ptr.dw);
  return;
}
