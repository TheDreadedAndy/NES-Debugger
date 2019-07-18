/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "../util/data.h"
#include "../util/util.h"
#include "./ppu.h"
#include "./palette.h"

/* Emulation constants */

// Object Attribute Memory size.
#define PRIMARY_OAM_SIZE 256U
#define SECONDARY_OAM_SIZE 32U

// The number of planes in a sprite or tile.
#define BIT_PLANES 2U

// Flags masks for the PPU status register.
#define FLAG_VBLANK 0x80U
#define FLAG_HIT 0x40U
#define FLAG_OVERFLOW 0x20U

// Flag masks for the PPU control register.
#define FLAG_ENABLE_VBLANK 0x80U
#define FLAG_SPRITE_SIZE 0x20U

/*
 * Sprite evaluation may perform several different actions independent of the
 * current cycle counter. These enums track which action should be performed.
 */
typedef enum eval_state {
  SCAN, COPY_TILE, COPY_ATTR, COPY_X, OVERFLOW, DONE
} eval_t;

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
  word_t oam_mask;
  word_t *primary_oam;
  word_t *secondary_oam;
  word_t *sprite_memory;

  // Temporary storage used in rendering.
  word_t next_tile[BIT_PLANES];
  word_t queued_bits[BIT_PLANES];
  word_t scrolling_bits[BIT_PLANES];
  word_t tile_palette;

  // Temporary storage used in sprite evaluation.
  word_t eval_buf;
  eval_t eval_state;
  word_t soam_addr;
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
void ppu_eval_clear_soam(void);
void ppu_eval_sprites(void);
word_t ppu_oam_read(void);
void ppu_eval_write_soam(void);
bool ppu_eval_in_range(void);
void ppu_eval_fetch_sprites(void);
void ppu_inc(void);

/*
 * Initializes the PPU and palette, using the given file, then creates an
 * SDL window.
 *
 * Assumes the file is non-NULL.
 */
void ppu_init(char *file) {
  // Prepare the ppu structure.
  system_ppu = xcalloc(1, sizeof(ppu_t));
  system_ppu->primary_oam = xcalloc(PRIMARY_OAM_SIZE, sizeof(word_t));
  system_ppu->secondary_oam = xcalloc(SECONDARY_OAM_SIZE, sizeof(word_t));
  system_ppu->sprite_memory = xcalloc(SECONDARY_OAM_SIZE, sizeof(word_t));

  // Load in the palette.
  palette_init(file);

  // Setup the SDL window.
  // TODO

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

  // Pull the NMI line high if one should be generated.
  ppu_signal();

  // Increment the cycle/scanline counters.
  ppu_inc();

  return;
}

/*
 * Runs the rendering action for the given cycle/scanline.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_render(void) {
  // Determine which scanline we're on and render accordingly.
  if (current_scanline < 240) {
    ppu_render_visible();
  } else if (current_scanline < 261) {
    ppu_render_blank();
  } else {
    ppu_render_pre();
  }

  return;
}

/*
 * TODO
 */
void ppu_render_visible(void) {
  return;
}

/*
 * Performs the rendering action during vertical blank, which consists only
 * of signaling an NMI on (1,241).
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_blank(void) {
  if (current_scanline == 241 && current_cycle == 1) {
    // TODO: Implement special case timing.
    system_ppu->status |= FLAG_VBLANK;
  }
  return;
}

/*
 * TODO
 */
void ppu_render_pre(void) {
  return;
}

/*
 * Runs the sprite evaluation action for the given cycle/scanline.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_eval(void) {
  // Sprite evaluation can activate an internal signal which sets all bits in
  // an OAM read.
  system_ppu->oam_mask = 0x00;

  // Sprite evaluation occurs only on visible scanlines.
  if (current_scanline >= 240) {
    // OAM addr is reset between 257 and 320 on the pre-render scanline.
    if (current_scanline >= 261 && current_cycle >= 257
                                && current_cycle <= 320) {
      // TODO
      system_ppu->oam_addr = 0;
    }
    return;
  }

  // Runs the sprite evaluation action for the given cycle.
  if (1 <= current_cycle && current_cycle <= 64) {
    ppu_eval_clear_soam();
  } else if (65 <= current_cycle && current_cycle <= 256) {
    ppu_eval_sprites();
  } else if (257 <= current_cycle && current_cycle <= 320) {
    // TODO
    system_ppu->oam_addr = 0;
    ppu_eval_fetch_sprites();
  }

  return;
}

/*
 * Sets every value in secondary OAM to 0xFF over the course of
 * 64 cycles. Used in sprite evaluation.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_eval_clear_soam(void) {
  // An internal signal which makes all primary OAM reads return 0xFF should
  // be raised during this phase of sprite evaluation.
  system_ppu->oam_mask = 0xFF;

  // The clear operation should consecutively write to secondary OAM every
  // even cycle.
  if ((current_cycle % 2) == 0) {
    word_t oam_addr = (current_cycle >> 1) & 0x1F;
    system_ppu->secondary_oam[oam_addr] = 0xFF;
  }

  return;
}

/*
 * Performs sprite evaluation for the given cycle, implementing the
 * incorrect sprite overflow behavior (partially). This function should
 * only be called from ppu_eval().
 *
 * Assumes the PPU has been initialized.
 * Assumes the first call on a scanline will happen on cycle 65.
 */
void ppu_eval_sprites(void) {
  // The evaluation state should be reset on its first cycle.
  if (current_cycle == 65) {
    system_ppu->eval_state = SCAN;
    system_ppu->soam_addr = 0;
  }

  // On odd cycles, data is read from primary OAM.
  if ((current_cycle % 2) == 1) {
    system_ppu->eval_buf = ppu_oam_read();
    return;
  }

  // We need to check for overflow to determine if evaluation has finished.
  word_t old_oam_addr = system_ppu->oam_addr;

  // On even cycles, the evaluation state determines the action.
  switch(system_ppu->eval_state) {
    case SCAN:
      // Copy the Y cord to secondary OAM and change state if the sprite
      // is visible on this scan line.
      system_ppu->secondary_oam[system_ppu->soam_addr] = system_ppu->eval_buf;
      if (ppu_eval_in_range()) {
        // Increment and prepare to copy the sprite data to secondary OAM.
        system_ppu->eval_state = COPY_TILE;
        system_ppu->oam_addr++;
        system_ppu->soam_addr++;
      } else {
        // Skip to next Y cord.
        system_ppu->oam_addr += 4;
      }
      break;
    case COPY_TILE:
      // Copy tile data from the buffer to secondary OAM, then update the state.
      ppu_eval_write_soam();
      system_ppu->eval_state = COPY_ATTR;
      break;
    case COPY_ATTR:
      // Copy attribute data to secondary OAM, then update the state.
      ppu_eval_write_soam();
      system_ppu->eval_state = COPY_X;
      break;
    case COPY_X:
      // Copy the X cord to secondary OAM, then update the state.
      ppu_eval_write_soam();
      if (system_ppu->soam_addr >= SECONDARY_OAM_SIZE) {
        system_ppu->eval_state = OVERFLOW;
      } else {
        system_ppu->eval_state = SCAN;
      }
      break;
    case OVERFLOW:
      // Run the broken overflow behavior until the end of OAM is reached.
      if (ppu_eval_in_range()) {
        system_ppu->status |= FLAG_OVERFLOW;
        system_ppu->eval_state = DONE;
      } else {
        // The NES incorrectly increments the OAM address by 5 instead of 4,
        // which results in broken sprite overflow behavior.
        system_ppu->oam_addr += 5;
      }
      break;
    case DONE:
      // Evaluation is finished, so we do nothing.
      break;
  }

  // If the primary OAM address overflows, evaluation is complete.
  if (old_oam_addr > system_ppu->oam_addr) { system_ppu->eval_state = DONE; }

  return;
}

/*
 * Reads a byte from primary OAM, accounting for the internal signal which may
 * force the value to 0xFF.
 *
 * Assumes the ppu has been initialized.
 */
word_t ppu_oam_read(void) {
  return system_ppu->primary_oam[system_ppu->oam_addr] & system_ppu->oam_mask;
}

/*
 * Performs a write to secondary OAM during sprite evaluation.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_eval_write_soam(void) {
  assert(system_ppu->soam_addr < SECONDARY_OAM_SIZE);
  system_ppu->secondary_oam[system_ppu->soam_addr] = system_ppu->eval_buf;
  system_ppu->oam_addr++;
  system_ppu->soam_addr++;
}

/*
 * Checks if a sprite is visible on the current scanline using the value
 * stored in the eval buffer as a Y cordinate.
 *
 * Assumes the PPU has been initialized.
 */
bool ppu_eval_in_range(void) {
  // Get the current size of sprites (8x8 or 8x16) from the control register
  // and the Y cord of the sprite from the eval buffer.
  word_t sprite_size = (system_ppu->ctrl & FLAG_SPRITE_SIZE) ? 16 : 8;
  word_t sprite_y = system_ppu->eval_buf;

  // Check if the sprite is visible on this scanline.
  bool in_range = (sprite_y <= current_scanline) && (sprite_y < 240)
                      && (current_scanline < sprite_y + sprite_size);
  return in_range;
}

/*
 * Copies all the sprites in secondary OAM to sprite memory. Should only
 * be called from ppu_eval().
 *
 * Assumes the PPU has been initialized.
 * Assumes the current cycle is between 257 and 320.
 */
void ppu_eval_fetch_sprites(void) {
  // Resets the address the first time this function is called for a scanline.
  if (current_cycle == 257) { system_ppu->soam_addr = 0; }

  // Sprite evaluation and rendering alternate access to sprite data every 4
  // cycles between 257 and 320.
  if (((current_cycle - 1) & 0x04) == 0) {
    assert(system_ppu->soam_addr < SECONDARY_OAM_SIZE);
    system_ppu->sprite_memory[system_ppu->soam_addr] =
                              system_ppu->secondary_oam[system_ppu->soam_addr];
    system_ppu->soam_addr++;
  }

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
  if ((current_cycle > 340) || ((current_cycle > 339)
          && frame_odd && (current_scanline >= 261))) {
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
  // Free the ppu structure.
  free(system_ppu->primary_oam);
  free(system_ppu->secondary_oam);
  free(system_ppu->sprite_memory);
  free(system_ppu);

  // Free the NES palette data.
  palette_free();

  return;
}
