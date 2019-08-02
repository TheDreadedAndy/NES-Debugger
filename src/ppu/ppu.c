/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "../util/data.h"
#include "../util/util.h"
#include "../util/contracts.h"
#include "./ppu.h"
#include "./palette.h"
#include "../cpu/2A03.h"
#include "../sdl/render.h"
#include "../memory/memory.h"

/* Emulation constants */

// Object Attribute Memory size.
#define PRIMARY_OAM_SIZE 256U
#define SECONDARY_OAM_SIZE 32U
#define SPRITE_DATA_SIZE 16U

// The number of planes in a sprite or tile.
#define BIT_PLANES 2U

// Mask for determining which register a mmio access is trying to use.
#define PPU_MMIO_MASK 0x0007U

// Flags masks for the PPU status register.
#define FLAG_VBLANK 0x80U
#define FLAG_HIT 0x40U
#define FLAG_OVERFLOW 0x20U

// PPU status is a 3 bit register.
#define PPU_STATUS_MASK 0xE0U

// Flag masks for the PPU control register.
#define FLAG_ENABLE_VBLANK 0x80U
#define FLAG_SPRITE_SIZE 0x20U
#define FLAG_VRAM_VINC 0x04U
#define FLAG_NAMETABLE 0x03U

/*
 * The lower three bits of an mmio access to the ppu determine which register
 * is used.
 */
#define PPU_CTRL_ACCESS 0
#define PPU_MASK_ACCESS 1
#define PPU_STATUS_ACCESS 2
#define OAM_ADDR_ACCESS 3
#define OAM_DATA_ACCESS 4
#define PPU_SCROLL_ACCESS 5
#define PPU_ADDR_ACCESS 6
#define PPU_DATA_ACCESS 7

// Masks for accessing the vram address register.
#define PPU_ADDR_HIGH_MASK 0x3F00U
#define PPU_ADDR_HIGH_SHIFT 8
#define PPU_ADDR_LOW_MASK 0x00FFU
#define VRAM_ADDR_MASK 0x3FFFU
#define PPU_PALETTE_OFFSET 0x3F00U

/*
 * Accesses to PPU scroll adjust the PPU address in non-standard ways.
 * These constants determine how to update the scroll value.
 */
#define SCROLL_X_MASK 0x001FU
#define SCROLL_Y_MASK 0x73E0U
#define SCROLL_VNT_MASK 0x0800U
#define SCROLL_HNT_MASK 0x0400U
#define SCROLL_NT_MASK (SCROLL_VNT_MASK | SCROLL_HNT_MASK)
#define SCROLL_NT_SHIFT 10
#define FINE_Y_SHIFT 12
#define FINE_X_MASK 0x0007U
#define COARSE_Y_SHIFT 2
#define COARSE_X_SHIFT 3
#define COARSE_Y_MASK 0x03E0U
#define COARSE_X_MASK 0x001FU

/*
 * The vram address of the ppu can be incremented based on the X and Y
 * scroll values. These constants facilitate that.
 */
#define FINE_Y_INC 0x1000U
#define FINE_Y_CARRY_MASK 0x8000U
#define FINE_Y_CARRY_SHIFT 10
#define COARSE_X_CARRY_MASK 0x0020U
#define Y_INC_OVERFLOW 0x03A0U
#define TOGGLE_HNT_SHIFT 4

/*
 * Sprite evaluation may perform several different actions independent of the
 * current cycle counter. These enums track which action should be performed.
 */
typedef enum eval_state {
  SCAN, COPY_TILE, COPY_ATTR, COPY_X, OVERFLOW, DONE
} eval_t;

/*
 * Contains the registers and memory internal to the PPU.
 */
typedef struct ppu {
  // Internal ppu registers.
  dword_t vram_addr;
  dword_t temp_vram_addr;
  bool write_toggle;
  word_t fine_x;

  // Memory mapped ppu registers.
  word_t bus;
  word_t vram_bus;
  word_t ctrl;
  word_t mask;
  word_t status;
  word_t oam_addr;

  // Working memory for the ppu.
  word_t oam_mask;
  word_t *primary_oam;
  word_t *secondary_oam;
  word_t *sprite_memory;
  word_t sprite_data[BIT_PLANES][SPRITE_DATA_SIZE];

  // Temporary storage used in rendering.
  word_t next_tile[BIT_PLANES];
  word_t queued_bits[BIT_PLANES];
  word_t scrolling_bits[BIT_PLANES];
  word_t tile_palette;

  // Temporary storage used in sprite evaluation.
  word_t eval_buf;
  eval_t eval_state;
  word_t soam_addr;

  // MDR and write toggle, used for 2-cycle r/w system.
  word_t mdr;
  bool mdr_write;
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
ppu_t *ppu = NULL;

/* Helper functions */
void ppu_render(void);
void ppu_render_visible(void);
void ppu_render_pixels(bool output);
void ppu_render_update_hori(void);
void ppu_render_prepare_sprites(void);
void ppu_render_prepare_bg(void);
void ppu_render_dummy_nametable(void);
void ppu_render_blank(void);
void ppu_render_pre(void);
void ppu_render_update_vert(void);
void ppu_eval(void);
void ppu_eval_clear_soam(void);
void ppu_eval_sprites(void);
word_t ppu_oam_read(void);
void ppu_eval_write_soam(void);
bool ppu_eval_in_range(void);
void ppu_eval_fetch_sprites(void);
void ppu_signal(void);
void ppu_inc(void);
void ppu_mmio_scroll_write(word_t val);
void ppu_mmio_addr_write(word_t val);
void ppu_mmio_vram_addr_inc(void);

/*
 * Initializes the PPU and palette, using the given file, then creates an
 * SDL window.
 *
 * Assumes the file is non-NULL.
 */
bool ppu_init(char *file) {
  // Prepare the ppu structure.
  ppu = xcalloc(1, sizeof(ppu_t));
  ppu->primary_oam = xcalloc(PRIMARY_OAM_SIZE, sizeof(word_t));
  ppu->secondary_oam = xcalloc(SECONDARY_OAM_SIZE, sizeof(word_t));
  ppu->sprite_memory = xcalloc(SECONDARY_OAM_SIZE, sizeof(word_t));

  // Load in the palette.
  if (!palette_init(file)) {
    fprintf(stderr, "Failed to initialize NES palette.\n");
    return false;
  }

  // Return success.
  return true;
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
 * Performs the current cycles action for a rendering scanline.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_visible(void) {
  /*
   * Determine which phase of rendering the scanline is in.
   * The first cycle may finish a read, when coming from a pre-render
   * line on an odd frame.
   */
  if (current_cycle == 0 && ppu->mdr_write) {
    // Finish the garbage nametable write that got skipped.
    ppu_render_dummy_nametable();
  } else if (current_cycle > 0 && current_cycle <= 256) {
    ppu_render_pixels(true);
  } else if (current_cycle > 256 && current_cycle <= 320) {
    // Fetch next sprite tile data.
    ppu_render_prepare_sprites();
  } else if (current_cycle > 320 && current_cycle <= 336) {
    // Fetch bg tile data.
    ppu_render_prepare_bg();
  } else if (current_cycle > 336 && current_cycle <= 340) {
    // Unused NT byte fetches, mappers may clock this.
    ppu_render_dummy_nametable();
  }

  return;
}

/*
 * TODO
 */
void ppu_render_pixels(bool output) {
  (void)output;
  return;
}

/*
 * Updates the horizontal piece of the vram address.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_update_hori(void) {
  // Copies the coarse X and horizontal nametable bit from t to v.
  ppu->vram_addr = (ppu->vram_addr & (SCROLL_Y_MASK | SCROLL_VNT_MASK))
                 | (ppu->temp_vram_addr & (SCROLL_X_MASK | SCROLL_HNT_MASK));
  return;
}

/*
 * Fetches the tile data for the sprites on the next scanline.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_prepare_sprites(void) {
  // The OAM address is reset to zero to prepare for sprite evaluation
  // on the next scanline.
  ppu->oam_addr = 0;

  // Determine if rendering has control of memory accesses,
  // or if evaluation is using it; since they share during these cycles.
  if ((current_cycle - 1) & 0x04) {
    // TODO: Need a way to determine how many sprites are on the scanline.
    // Once I have this, pattern data can be safely copied.
  }

  return;
}

/*
 * TODO
 */
void ppu_render_prepare_bg(void) {
  return;
}

/*
 * Executes 2 dummy nametable fetches over 4 cycles.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_dummy_nametable(void) {
  // Determine which cycle of the fetch we are on.
  if (ppu->mdr_write) {
    // Second cycle, thrown away internally.
    ppu->mdr_write = false;
  } else {
    // First cycle, reads from vram.
    ppu->mdr = memory_vram_read(ppu->vram_addr);
    ppu->mdr_write = true;
  }

  return;
}

/*
 * Performs a coarse X increment on the vram address. Accounts for the
 * nametable bits.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_xinc(void) {
  // Increment the coarse X.
  dword_t xinc = (ppu->vram_addr & COARSE_X_MASK) + 1;

  // When coarse X overflows, bit 10 (horizontal nametable select) is flipped.
  ppu->vram_addr = ((ppu->vram_addr & ~COARSE_X_MASK)
                 | (xinc & COARSE_X_MASK))
                 ^ ((xinc & COARSE_X_CARRY_MASK) << TOGGLE_HNT_SHIFT);
  return;
}

/*
 * Performs a Y increment on the vram address. Accounts for both coarse Y,
 * fine Y, and the nametable bits.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_yinc(void) {
  // Increment fine Y.
  ppu->vram_addr = (ppu->vram_addr & VRAM_ADDR_MASK) + FINE_Y_INC;

  // Add overflow to coarse Y.
  ppu->vram_addr = (ppu->vram_addr & ~COARSE_Y_MASK)
                 | ((ppu->vram_addr + ((ppu->vram_addr & FINE_Y_CARRY_MASK)
                 >> FINE_Y_CARRY_SHIFT)) & COARSE_Y_MASK);

  // The vertical name table bit should be toggled if coarse Y was incremented
  // to 30.
  if ((ppu->vram_addr & SCROLL_Y_MASK) == Y_INC_OVERFLOW) {
    ppu->vram_addr ^= SCROLL_VNT_MASK;
  }

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
    ppu->status |= FLAG_VBLANK;
    render_frame();
  }
  return;
}

/*
 * Execute the pre-render scanline acording to the current cycle.
 * Resets to the status flags are performed here.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_pre(void) {
  // Determine which phase of rendering the scanline is in.
  if (current_cycle > 0 && current_cycle <= 256) {
    // Accesses are made, but nothing is rendered.
    ppu_render_pixels(false);

    // The status flags are reset at the begining of the pre-render scanline.
    ppu->status = 0;
  } else if (current_cycle >= 280 && current_cycle <= 304) {
    // Update the vertical part of the vram address register.
    ppu_render_update_vert();
  } else if (current_cycle > 320 && current_cycle <= 336) {
    // Fetch bg tile data.
    ppu_render_prepare_bg();
  } else if (current_cycle > 336 && current_cycle <= 340) {
    // Unused NT byte fetches, mappers may clock this.
    ppu_render_dummy_nametable();
  }

  // The OAM addr is reset here to prepare for sprite evaluation on the next
  // scanline.
  if (current_cycle > 256 && current_cycle <= 320) { ppu->oam_addr = 0; }

  return;
}

/*
 * Updates the vertical piece of the vram address.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_update_vert(void) {
  // Copies the fine Y, coarse Y, and vertical nametable bit from t to v.
  ppu->vram_addr = (ppu->vram_addr & (SCROLL_X_MASK | SCROLL_HNT_MASK))
                 | (ppu->temp_vram_addr & (SCROLL_Y_MASK | SCROLL_VNT_MASK));
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
  ppu->oam_mask = 0x00;

  // Sprite evaluation occurs only on visible scanlines.
  if (current_scanline >= 240) { return; }

  // Runs the sprite evaluation action for the given cycle.
  if (1 <= current_cycle && current_cycle <= 64) {
    ppu_eval_clear_soam();
  } else if (65 <= current_cycle && current_cycle <= 256) {
    ppu_eval_sprites();
  } else if (257 <= current_cycle && current_cycle <= 320) {
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
  ppu->oam_mask = 0xFF;

  // The clear operation should consecutively write to secondary OAM every
  // even cycle.
  if ((current_cycle % 2) == 0) {
    word_t oam_addr = (current_cycle >> 1) & 0x1F;
    ppu->secondary_oam[oam_addr] = 0xFF;
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
    ppu->eval_state = SCAN;
    ppu->soam_addr = 0;
  }

  // On odd cycles, data is read from primary OAM.
  if ((current_cycle % 2) == 1) {
    ppu->eval_buf = ppu_oam_read();
    return;
  }

  // We need to check for overflow to determine if evaluation has finished.
  word_t old_oam_addr = ppu->oam_addr;

  // On even cycles, the evaluation state determines the action.
  switch(ppu->eval_state) {
    case SCAN:
      // Copy the Y cord to secondary OAM and change state if the sprite
      // is visible on this scan line.
      ppu->secondary_oam[ppu->soam_addr] = ppu->eval_buf;
      if (ppu_eval_in_range()) {
        // Increment and prepare to copy the sprite data to secondary OAM.
        ppu->eval_state = COPY_TILE;
        ppu->oam_addr++;
        ppu->soam_addr++;
      } else {
        // Skip to next Y cord.
        ppu->oam_addr += 4;
      }
      break;
    case COPY_TILE:
      // Copy tile data from the buffer to secondary OAM, then update the state.
      ppu_eval_write_soam();
      ppu->eval_state = COPY_ATTR;
      break;
    case COPY_ATTR:
      // Copy attribute data to secondary OAM, then update the state.
      ppu_eval_write_soam();
      ppu->eval_state = COPY_X;
      break;
    case COPY_X:
      // Copy the X cord to secondary OAM, then update the state.
      ppu_eval_write_soam();
      if (ppu->soam_addr >= SECONDARY_OAM_SIZE) {
        ppu->eval_state = OVERFLOW;
      } else {
        ppu->eval_state = SCAN;
      }
      break;
    case OVERFLOW:
      // Run the broken overflow behavior until the end of OAM is reached.
      if (ppu_eval_in_range()) {
        ppu->status |= FLAG_OVERFLOW;
        ppu->eval_state = DONE;
      } else {
        // The NES incorrectly increments the OAM address by 5 instead of 4,
        // which results in broken sprite overflow behavior.
        ppu->oam_addr += 5;
      }
      break;
    case DONE:
      // Evaluation is finished, so we do nothing.
      break;
  }

  // If the primary OAM address overflows, evaluation is complete.
  if (old_oam_addr > ppu->oam_addr) { ppu->eval_state = DONE; }

  return;
}

/*
 * Reads a byte from primary OAM, accounting for the internal signal which may
 * force the value to 0xFF.
 *
 * Assumes the ppu has been initialized.
 */
word_t ppu_oam_read(void) {
  return ppu->primary_oam[ppu->oam_addr] & ppu->oam_mask;
}

/*
 * Performs a write to secondary OAM during sprite evaluation.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_eval_write_soam(void) {
  assert(ppu->soam_addr < SECONDARY_OAM_SIZE);
  ppu->secondary_oam[ppu->soam_addr] = ppu->eval_buf;
  ppu->oam_addr++;
  ppu->soam_addr++;
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
  word_t sprite_size = (ppu->ctrl & FLAG_SPRITE_SIZE) ? 16 : 8;
  word_t sprite_y = ppu->eval_buf;

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
  if (current_cycle == 257) { ppu->soam_addr = 0; }

  // Sprite evaluation and rendering alternate access to sprite data every 4
  // cycles between 257 and 320.
  if (((current_cycle - 1) & 0x04) == 0) {
    CONTRACT(ppu->soam_addr < SECONDARY_OAM_SIZE);
    ppu->sprite_memory[ppu->soam_addr] = ppu->secondary_oam[ppu->soam_addr];
    ppu->soam_addr++;
  }

  return;
}

/*
 * Generates an NMI in the CPU emulation when appropriate.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_signal(void) {
  // NMIs should be generated when they are enabled in ppuctrl and
  // the ppu is in vblank.
  nmi_line = (ppu->ctrl & FLAG_ENABLE_VBLANK) && (ppu->status & FLAG_VBLANK);
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
  // Fill the PPU bus with the value being written.
  ppu->bus = val;

  // Determine which register is being accessed.
  switch(reg_addr & PPU_MMIO_MASK) {
    case PPU_CTRL_ACCESS:
      ppu->ctrl = val;
      // Update the scrolling nametable selection.
      ppu->temp_vram_addr = (ppu->temp_vram_addr & ~SCROLL_NT_MASK)
               | (((dword_t) (ppu->ctrl & FLAG_NAMETABLE)) << SCROLL_NT_SHIFT);
      break;
    case PPU_MASK_ACCESS:
      ppu->mask = val;
      break;
    case PPU_STATUS_ACCESS:
      // Read only.
      break;
    case OAM_ADDR_ACCESS:
      // TODO: OAM Corruption on write.
      ppu->oam_addr = val;
      break;
    case OAM_DATA_ACCESS:
      ppu->primary_oam[ppu->oam_addr] = val;
      // TODO: OAM write increments should be wrong during rendering.
      ppu->oam_addr++;
      break;
    case PPU_SCROLL_ACCESS:
      ppu_mmio_scroll_write(val);
      break;
    case PPU_ADDR_ACCESS:
      ppu_mmio_addr_write(val);
      break;
    case PPU_DATA_ACCESS:
      memory_vram_write(val, ppu->vram_addr);
      ppu_mmio_vram_addr_inc();
      break;
  }

  return;
}

/*
 * Writes the the PPU scroll register. Toggles the write bit.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_mmio_scroll_write(word_t val) {
  // Determine which write should be done.
  if (ppu->write_toggle) {
    // Update scroll X.
    ppu->fine_x = val & FINE_X_MASK;
    ppu->temp_vram_addr = (ppu->temp_vram_addr & SCROLL_Y_MASK)
                        | (ppu->temp_vram_addr & SCROLL_NT_MASK)
                        | (val >> COARSE_X_SHIFT);
  } else {
    // Update scroll Y.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & SCROLL_X_MASK)
             | (ppu->temp_vram_addr & SCROLL_NT_MASK)
             | ((((dword_t) val) << COARSE_Y_SHIFT) & COARSE_Y_MASK)
             | (((dword_t) val) << FINE_Y_SHIFT);
  }

  // Toggle the write bit.
  ppu->write_toggle = !(ppu->write_toggle);
  return;
}

/*
 * Writes to the PPU addr register. Toggles the write bit.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_mmio_addr_write(word_t val) {
  // Determine which write should be done.
  if (ppu->write_toggle) {
    // Write the low byte and update v.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & PPU_ADDR_HIGH_MASK) | val;
    ppu->vram_addr = ppu->temp_vram_addr;
  } else {
    // Write the high byte.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & PPU_ADDR_LOW_MASK)
        | ((((dword_t) val) << PPU_ADDR_HIGH_SHIFT) & PPU_ADDR_HIGH_MASK);
  }

  // Toggle the write bit.
  ppu->write_toggle = !(ppu->write_toggle);

  return;
}

/*
 * Increments the vram address after a PPU data access, implementing the
 * buggy behavior expected during rendering.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_mmio_vram_addr_inc(void) {
  // Determine how the increment should work.
  if (current_scanline >= 240 && current_scanline <= 260) {
    ppu->vram_addr += (ppu->ctrl & FLAG_VRAM_VINC) ? 32 : 1;
  } else if (!(((current_cycle > 0 && current_cycle <= 256)
         || current_cycle > 320) && (current_cycle % 8) == 0)) {
    // Writing to PPU data during rendering causes a X and Y increment.
    // This only happens when the PPU would not otherwise be incrementing them.
    ppu_render_yinc();
    ppu_render_xinc();
  }

  return;
}

/*
 * Takes in an address from cpu memory and uses it to read from
 * the corresponding mmio in the ppu.
 *
 * Assumes the ppu has been initialized.
 */
word_t ppu_read(dword_t reg_addr) {
  // Determine which register is being read from.
  switch(reg_addr & PPU_MMIO_MASK) {
    case PPU_STATUS_ACCESS:
      // PPU status contains only 3 bits (high 3).
      ppu->bus = (ppu->bus & ~PPU_STATUS_MASK)
               | (ppu->status & PPU_STATUS_MASK);
      // Reads to PPU status reset the write toggle and clear the vblank flag.
      ppu->status &= ~FLAG_VBLANK;
      ppu->write_toggle = false;
      break;
    case OAM_DATA_ACCESS:
      ppu->bus = ppu->primary_oam[ppu->oam_addr];
      break;
    case PPU_DATA_ACCESS:
      // Reading from mappable VRAM (not the palette) returns the value to an
      // internal bus.
      if (reg_addr < PPU_PALETTE_OFFSET) {
        ppu->bus = ppu->vram_bus;
        ppu->vram_bus = memory_vram_read(reg_addr);
      } else {
        ppu->bus = memory_vram_read(reg_addr);
      }
      break;
    case PPU_CTRL_ACCESS:
    case PPU_MASK_ACCESS:
    case OAM_ADDR_ACCESS:
    case PPU_SCROLL_ACCESS:
    case PPU_ADDR_ACCESS:
    default:
      // Reading from a write only register simply yields the value on the bus.
      break;
  }

  // The requested value should now be on the bus, so we return.
  return ppu->bus;
}

/*
 * Directly writes the given value to OAM, incrementing the OAM address.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_oam_dma(word_t val) {
  ppu->primary_oam[ppu->oam_addr] = val;
  ppu->oam_addr++;
  return;
}

/*
 * Frees the global ppu structure.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_free(void) {
  // Free the ppu structure.
  free(ppu->primary_oam);
  free(ppu->secondary_oam);
  free(ppu->sprite_memory);
  free(ppu);

  // Free the NES palette data.
  palette_free();

  return;
}
