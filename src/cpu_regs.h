#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

#ifndef _NES_REGS
#define _NES_REGS

typedef struct regfile {
  // Standard 6502 registers.
  uint8_t A;
  uint8_t X;
  uint8_t Y;
  uint8_t S;
  uint8_t P;
  uint8_t PCL;
  uint8_t PCH;
  uint8_t inst;
  // These registers didn't necessarily exist in the 6502, but are used to
  // manage the machine state.
  uint8_t MDR;
  uint8_t addrL;
  uint8_t addrH;
  uint8_t ptrL;
  uint8_t ptrH;
  uint8_t carry;
} regfile_t;

// Creates a regfile. Regfiles can be freed by free.
regfile_t *regfile_new(memory_t *M);

// Prints out the contents of the register file.
void regfile_print(regfile_t *R, size_t i);

#endif
