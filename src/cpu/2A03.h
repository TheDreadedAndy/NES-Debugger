#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "./state.h"
#include "../memory/header.h"

#ifndef _NES_2A03
#define _NES_2A03

// Interrupt bools, which can be set by the PPU/APU
extern bool irq_interrupt, nmi_interrupt;

// Interrupt bools, which can be read (and should not be set) by the micro ops.
extern bool nmi_edge, irq_ready;

// Initializes the cpu, so that cycles may be executed.
void cpu_init(FILE *rom_file, header_t *header);

// Executes the next cycle of the 2A03.
void cpu_run_cycle(void);

// Fetches the next instruction in the emulation. Should only be called from
// micro ops.
void cpu_fetch(micro_t *micro);

// Frees anything related to the cpu emulation.
void cpu_free(void);

#endif
