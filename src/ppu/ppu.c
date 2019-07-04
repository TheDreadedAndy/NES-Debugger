/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"
#include "./ppu.h"

/*
 * TODO: Document better.
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

/*
 * The actions performed by the ppu each cycle depend entirely on which cycle
 * and scanline it is on. These variables track that information so that the
 * ppu emulation can adjust acordingly.
 */
size_t current_scanline = 261;
size_t current_cycle = 0;
size_t frame_odd = false;

/*
 * TODO
 */
void ppu_init(void) {
  return;
}

/*
 * TODO
 */
void ppu_run_cycle(void) {
  return;
}

/*
 * Takes in an address from cpu memory and uses it to write to the
 * corresponding mmio in the ppu.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_write(dword_t reg_addr, word_t val) {
  (void)reg_addr;
  (void)val;
  return;
}

/*
 * Takes in an address from cpu memory and uses it to read from
 * the corresponding mmio in the ppu.
 *
 * Assumes the ppu has been initialized.
 */
word_t ppu_read(dword_t reg_addr) {
  (void)reg_addr;
  return 0;
}

/*
 * Directly writes the given value to OAM, incrementing the OAM address.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_oam_dma(word_t val) {
  (void)val;
  return;
}

/*
 * TODO
 */
void ppu_free(void) {
  return;
}
