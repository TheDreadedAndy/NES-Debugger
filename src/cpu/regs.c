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
regfile_t *system_regfile = NULL;

/*
 * Initializes the global register file.
 *
 * Assumes the register file has not already been initialized.
 */
void regfile_init(void) {
  system_regfile = xcalloc(1, sizeof(regfile_t));
  return;
}

/*
 * Increments the PC registers in a regfile.
 * Assumes the regfile is non-null.
 */
void regfile_inc_pc(void) {
  dword_t pc = get_dword(system_regfile->pc_lo, system_regfile->pc_hi);
  pc++;
  system_regfile->pc_lo = (word_t)pc;
  system_regfile->pc_hi = (word_t)(pc >> 8);
  return;
}

/*
 * Prints a regfile.
 */
void regfile_print(size_t i) {
  printf("-----------------------------------\n");
  printf("State following iteration %d:\n", (int)i);
  printf("A: %x, X: %x, Y: %x, INST: %x\n", system_regfile->A,
         system_regfile->X, system_regfile->Y, system_regfile->inst);
  printf("State (flags): %x, Stack pointer: %x\n", system_regfile->P,
         system_regfile->S);
  printf("PCL: %x, PCH: %x\n", system_regfile->pc_lo, system_regfile->pc_hi);
  printf("Abstraction register state:\n");
  printf("MDR: %x, Carry: %x\n", system_regfile->mdr, system_regfile->carry);
  printf("Addr Low: %x, Addr High: %x\n", system_regfile->addr_lo,
         system_regfile->addr_hi);
  printf("Pointer Low: %x, Pointer High: %x\n", system_regfile->ptr_lo,
         system_regfile->ptr_hi);
  return;
}
