#include "../util/data.h"

#ifndef _NES_PPU
#define _NES_PPU

// Inits the ppu structure, allowing the ppu to be used.
void ppu_init(char *file);

// Runs the next ppu cycle.
void ppu_run_cycle(void);

// Write to a memory mapped ppu register, handling mirroring.
void ppu_write(dword_t reg_addr, word_t val);

// Read from a memory mapped ppu register, handling mirroring.
word_t ppu_read(dword_t reg_addr);

// Directly write to OAM with the given value.
// The current oam address is incremented by this operation.
void ppu_oam_dma(word_t val);

// Frees any memory the ppu was using.
void ppu_free(void);

#endif
