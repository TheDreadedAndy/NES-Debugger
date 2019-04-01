#include <stdlib.h>
#include <stdint.h>
#include "memory.h"

#ifndef _NES_2A03
#define _NES_2A03

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

// Interrupt bools, which can be set by the PPU/APU
extern bool IRQ, NMI;

// Executes the next cycle of the 2A03.
void execute(regfile_t *R, memory_t *M);

#endif
