#include <stdlib.h>
#include <stdint.h>
#include "../memory/memory.h"
#include "../util/data.h"

#ifndef _NES_REGS
#define _NES_REGS

typedef struct regfile {
  // Standard 6502 registers.
  word_t A;
  word_t X;
  word_t Y;
  word_t S;
  word_t P;
  mword_t pc;
  word_t inst;
  // These registers didn't necessarily exist in the 6502, but are used to
  // manage the machine state.
  word_t mdr;
  mword_t addr;
  mword_t ptr;
  word_t carry;
} regfile_t;

/* Global regfile, used in CPU emulation. */
extern regfile_t *R;

// Creates a regfile. regfiles can be freed by free.
void regfile_init();

// Increments the PC registers in the regfile.
void regfile_inc_pc();

// Prints out the contents of the register file.
void regfile_print(size_t i);

#endif
