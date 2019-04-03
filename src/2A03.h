#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "memory.h"
#include "cpu_regs.h"
#include "state.h"

#ifndef _NES_2A03
#define _NES_2A03

// Interrupt bools, which can be set by the PPU/APU
extern bool IRQ, NMI;

// Executes the next cycle of the 2A03.
void cpu_run_cycle(regfile_t *R, memory_t *M, state_t *S);

#endif
