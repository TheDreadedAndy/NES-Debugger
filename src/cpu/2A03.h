#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../memory/header.h"
#include "./regs.h"
#include "./state.h"

#ifndef _NES_2A03
#define _NES_2A03

// Interrupt bools, which can be set by the PPU/APU
extern bool irq_interrupt, nmi_interrupt;

// Initializes the cpu, so that cycles may be executed.
void cpu_init(FILE *rom_file, header_t *header);

// Executes the next cycle of the 2A03.
void cpu_run_cycle(void);

// Frees anything related to the cpu emulation.
void cpu_free(void);

#endif
