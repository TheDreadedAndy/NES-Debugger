#include "../util/data.h"

/*
 * TODO: Document better, move to ppu.c.
 */
typedef struct ppu {
  // Internal ppu registers.
  dword_t ppu_vram_addr;
  dword_t ppu_temp_vram_addr;
  bool ppu_write_toggle;
  word_t ppu_fine_x;

  // Memory mapped ppu registers.
  word_t ppu_bus;
  word_t ppu_ctrl;
  word_t ppu_mask;
  word_t ppu_status;
  word_t ppu_oam_addr;

  // Working memory for the ppu.
  word_t *ppu_primary_oam;
  word_t *ppu_secondary_oam;
} ppu_t;

// Inits the ppu structure, allowing the ppu to be used.
void ppu_init(void);

// Runs the next ppu cycle.
void ppu_run_cycle(void);

// Write to a memory mapped ppu register, handling mirroring.
void ppu_write(dword_t reg_addr, word_t val);

// Read from a memory mapped ppu register, handling mirroring.
word_t ppu_read(dword_t reg_addr);

// Directly write to OAM with the given value.
// The current oam address is incremented by this operation.
void ppu_oam_dma(word_t val);
