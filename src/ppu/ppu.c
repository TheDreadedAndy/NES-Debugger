/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"
#include "../util/util.h"
#include "./ppu.h"

/*
 * TODO: Document better.
 */
typedef struct ppu {
  // Internal ppu registers.
  dword_t vram_addr;
  dword_t temp_vram_addr;
  bool write_toggle;
  word_t fine_x;

  // Memory mapped ppu registers.
  word_t bus;
  word_t ctrl;
  word_t mask;
  word_t status;
  word_t oam_addr;

  // Working memory for the ppu.
  word_t *primary_oam;
  word_t *secondary_oam;
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
 * Global ppu structure. Cannot be accessed outside this file.
 * Initialized by ppu_init().
 */
ppu_t *system_ppu = NULL;

/* Helper functions */
void ppu_render(void);
void ppu_eval(void);
void ppu_inc(void);

/*
 * Initializes the ppu by creating a ppu structure and storing it in
 * system_ppu.
 */
void ppu_init(void) {
  system_ppu = xcalloc(1, sizeof(ppu_t));
  system_ppu->primary_oam = xcalloc(PRIMARY_OAM_SIZE, sizeof(word_t));
  system_ppu->secondary_oam = xcalloc(SECONDARY_OAM_SIZE, sizeof(word_t));
  return;
}

/*
 * Runs the next cycle in the ppu emulation, then increments the cycle/scanline
 * counters.
 *
 * Assumes the ppu and memory have been initialized.
 */
void ppu_run_cycle(void) {
  // Render video using the current scanline/cycle.
  ppu_render();

  // Run sprite evaluation using the current scanline/cycle.
  ppu_eval();

  // Increment the cycle/scanline counters.
  ppu_inc();

  return;
}

/*
 * TODO
 */
void ppu_render(void) {
  return;
}

/*
 * TODO
 */
void ppu_eval(void) {
  return;
}

/*
 * Increments the scanline, cycle, and frame type and correctly wraps them.
 * Each ppu frame has 341 cycles and 262 scanlines.
 */
void ppu_inc(void) {
  // Increment the cycle.
  current_cycle++;

  // Increment the scanline if it is time to wrap the cycle.
  if ((current_cycle > 341) || ((current_cycle > 339)
           && frame_odd && (current_scanline >= 261)) {
    current_scanline++;
    current_cycle = 0;
  }

  // Wrap the scanline and toggle the frame if it is time to do so.
  if (current_scanline > 261) {
    current_scanline = 0;
    frame_odd = !frame_odd;
  }

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
  system_ppu->primary_oam[system_ppu->oam_addr] = val;
  system_ppu->oam_addr++;
  return;
}

/*
 * Frees the global ppu structure.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_free(void) {
  free(system_ppu->primary_oam);
  free(system_ppu->secondary_oam);
  free(system_ppu);
  return;
}
