/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
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
#define SPRITE_DATA_SIZE 8U

// The number of planes in a sprite or tile.
#define BIT_PLANES 2U

// Mask for determining which register a mmio access is trying to use.
#define PPU_MMIO_MASK 0x0007U

// Flag masks for the PPU status register.
#define FLAG_VBLANK 0x80U
#define FLAG_HIT 0x40U
#define FLAG_OVERFLOW 0x20U

// PPU status is a 3 bit register.
#define PPU_STATUS_MASK 0xE0U

// Flag masks for the PPU mask register.
#define FLAG_FOCUS_BLUE 0x80U
#define FLAG_FOCUS_GREEN 0x40U
#define FLAG_FOCUS_RED 0x20U
#define FLAG_RENDER_SPRITES 0x10U
#define FLAG_RENDER_BG 0x08U
#define FLAG_SHOW_SPRITES 0x04U
#define FLAG_SHOW_BG 0x02U
#define FLAG_GREYSCALE 0x01U

// Flag masks for the PPU control register.
#define FLAG_ENABLE_VBLANK 0x80U
#define FLAG_SPRITE_SIZE 0x20U
#define FLAG_BG_TABLE 0x10U
#define FLAG_SPRITE_TABLE 0x08U
#define FLAG_VRAM_VINC 0x04U
#define FLAG_NAMETABLE 0x03U

// Flag masks for sprite attribute data (in OEM).
#define FLAG_SPRITE_VFLIP 0x80U
#define FLAG_SPRITE_HFLIP 0x40U
#define FLAG_SPRITE_PRIORITY 0x20U
#define FLAG_SPRITE_PALETTE 0x03U

// Used to form a pattern table address for a tile byte.
#define PATTERN_TABLE_LOW 0x0000U
#define PATTERN_TABLE_HIGH 0x1000U

// Used to form a pattern table address, for a sprite, from sprite memory.
#define X16_INDEX_OFFSET 0x10U
#define X16_PLANE_SHIFT 3
#define X16_TILE_MASK 0xFEU
#define X16_TILE_SHIFT 4
#define X16_TABLE_MASK 0x01U
#define X16_TABLE_SHIFT 12
#define X8_PLANE_SHIFT 3
#define X8_TILE_SHIFT 4
#define X8_TABLE_SHIFT 9

/*
 * Used, during rendering, to determine which piece of the tile should be
 * loaded on a given cycle.
 */
#define REG_UPDATE_MASK 0x07U
#define REG_FETCH_NT 1U
#define REG_FETCH_AT 3U
#define REG_FETCH_TILE_LOW 5U
#define REG_FETCH_TILE_HIGH 7U

// Used to calculate the attribute table address.
#define ATTRIBUTE_BASE_ADDR 0x23C0U

// Constants used during the rendering of pixels.
#define PALETTE_BASE_ADDR 0x3F00U
#define SPRITE_PALETTE_BASE 0x10U
#define GREYSCALE_PIXEL_MASK 0x30U

/*
 * The lower three bits of an mmio access to the ppu determine which register
 * is used.
 */
#define PPU_CTRL_ACCESS 0U
#define PPU_MASK_ACCESS 1U
#define PPU_STATUS_ACCESS 2U
#define OAM_ADDR_ACCESS 3U
#define OAM_DATA_ACCESS 4U
#define PPU_SCROLL_ACCESS 5U
#define PPU_ADDR_ACCESS 6U
#define PPU_DATA_ACCESS 7U

// Masks for accessing the vram address register.
#define PPU_ADDR_HIGH_MASK 0x3F00U
#define PPU_ADDR_HIGH_SHIFT 8
#define PPU_ADDR_LOW_MASK 0x00FFU
#define VRAM_ADDR_MASK 0x7FFFU
#define VRAM_BUS_MASK 0x3FFFU
#define PPU_PALETTE_OFFSET 0x3F00U
#define PPU_NT_OFFSET 0x2000U
#define VRAM_NT_ADDR_MASK 0x0FFFU

/*
 * Accesses to PPU scroll adjust the PPU address in non-standard ways.
 * These constants determine how to update the scroll value.
 */
#define SCROLL_X_MASK 0x001FU
#define SCROLL_Y_MASK 0x73E0U
#define SCROLL_VNT_MASK 0x0800U
#define SCROLL_HNT_MASK 0x0400U
#define SCROLL_NT_MASK 0x0C00U
#define SCROLL_NT_SHIFT 10
#define FINE_Y_MASK 0x7000U
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
#define Y_INC_OVERFLOW 0x03C0U
#define TOGGLE_HNT_SHIFT 5

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
  word_t sprite_data[SPRITE_DATA_SIZE][BIT_PLANES];
  word_t sprite_count;
  word_t next_sprite_count;
  word_t zero_index;
  bool zero_in_soam;
  bool zero_in_mem;

  // Temporary storage used in rendering.
  word_t next_tile[BIT_PLANES];
  word_t queued_bits[BIT_PLANES];
  word_t tile_scroll[BIT_PLANES];
  word_t palette_scroll[BIT_PLANES];
  word_t palette_latch[BIT_PLANES];
  word_t next_palette[BIT_PLANES];

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
bool frame_odd = false;

/*
 * Global ppu structure. Cannot be accessed outside this file.
 * Initialized by ppu_init().
 */
ppu_t *ppu = NULL;

/* Helper functions */
bool ppu_is_disabled(void);
void ppu_disabled(void);
void ppu_render(void);
void ppu_render_visible(void);
void ppu_render_update_frame(bool output);
void ppu_render_draw_pixel(void);
word_t ppu_render_get_tile_pixel(void);
word_t ppu_render_get_sprite_pixel(void);
word_t ppu_render_get_sprite_index(void);
void ppu_render_update_registers(void);
void ppu_render_get_attribute(void);
word_t ppu_render_get_tile(word_t index, bool plane_high);
void ppu_render_update_hori(void);
void ppu_render_prepare_sprites(void);
word_t ppu_render_get_sprite(void);
void ppu_render_prepare_bg(void);
void ppu_render_dummy_nametable_access(void);
void ppu_render_xinc(void);
void ppu_render_yinc(void);
void ppu_render_blank(void);
void ppu_render_pre(void);
void ppu_render_update_vert(void);
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
  // Check if rendering has been disabled by the software.
  if (ppu_is_disabled()) {
    ppu_disabled();
  } else {
    // Render video using the current scanline/cycle.
    ppu_render();
  }

  // Pull the NMI line high if one should be generated.
  ppu_signal();

  // Increment the cycle/scanline counters.
  ppu_inc();

  return;
}

/*
 * Returns true when rendering is disabled.
 *
 * Assumes the ppu has been initialized.
 */
bool ppu_is_disabled(void) {
  return !(ppu->mask & FLAG_RENDER_BG) && !(ppu->mask & FLAG_RENDER_SPRITES);
}

/*
 * Performs any PPU actions which cannot be disabled.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_disabled(void) {
  // The OAM mask should not be active when rendering is disabled.
  ppu->oam_mask = 0;

  // Determine which action should be performed.
  if ((current_scanline < 240) && (current_cycle > 0)
                               && (current_cycle <= 256)) {
    // Draws the background color to the screen when rendering is disabled.
    ppu_render_draw_pixel();
  } else if (current_scanline < 261) {
    // Signals the start of vblanks.
    ppu_render_blank();
  } else if (current_cycle == 1) {
    // Resets the status register during the pre-render scanline.
    ppu->status = 0;
  }

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
  // Sprite evaluation can raise an internal signal that forces OAM reads
  // to return 0xFF. In the emulation, this signal is reset each cycle and
  // set by ppu_eval_clear_soam() when it needs to be raised.
  ppu->oam_mask = 0x00;

  /*
   * Determine which phase of rendering the scanline is in.
   * The first cycle may finish a read, when coming from a pre-render scanline
   * on an odd frame.
   */
  if (current_cycle == 0) {
    // Finish the garbage nametable write that got skipped.
    if (ppu->mdr_write) { ppu_render_dummy_nametable_access(); }
  } else if (current_cycle <= 64) {
    // Draw a pixel in the frame.
    ppu_render_update_frame(true);
    // Clear SOAM for sprite evaluation.
    ppu_eval_clear_soam();
  } else if (current_cycle <= 256) {
    // Draw a pixel in the frame.
    ppu_render_update_frame(true);
    // Evaluate the sprites on the next scanline.
    ppu_eval_sprites();
  } else if (current_cycle <= 320) {
    // On cycle 257, the horizontal vram position is loaded from the temp vram
    // address register.
    if (current_cycle == 257) { ppu_render_update_hori(); }
    // Fetch the sprite data for the next scanline.
    ppu_eval_fetch_sprites();
    // Fetch next sprite tile data.
    ppu_render_prepare_sprites();
  } else if (current_cycle <= 336) {
    // Fetch the background tile data for the next cycle.
    ppu_render_update_registers();
    if ((current_cycle % 8) == 0) { ppu_render_xinc(); }
  } else if (current_cycle > 336 && current_cycle <= 340) {
    // Unused NT byte fetches, mappers may clock this.
    ppu_render_dummy_nametable_access();
  }

  return;
}

/*
 * Runs a ppu cycle during the drawing phase of a visible scanline.
 * This includes drawing a pixel to the screen, updating the shift registers,
 * and possibly incrementing the vram address.
 *
 * Assumes the ppu has been initialized and is currently between cycles
 * 1 and 256 (inclusive) of a visible scanline.
 */
void ppu_render_update_frame(bool output) {
  // Render the pixel.
  if (output) { ppu_render_draw_pixel(); }

  // Update the background registers.
  ppu_render_update_registers();

  // Update the vram address.
  if ((current_cycle % 8) == 0) {
    // Every 8 cycles, the horizontal vram position is incremented.
    ppu_render_xinc();
    // On cycle 256, the vertical vram position is incremented.
    if (current_cycle == 256) { ppu_render_yinc(); }
  }

  return;
}

/*
 * Uses the shift registers and sprite memory to draw the current cycles
 * pixel to the screen
 *
 * Assumes the PPU has been initialized and is currently running a cycle
 * between 1 and 256 (inclusive) of a visible scanline.
 */
void ppu_render_draw_pixel(void) {
  // Get the universal background color address and the screen position.
  size_t screen_x = current_cycle - 1;
  size_t screen_y = current_scanline;
  dword_t color_addr = PALETTE_BASE_ADDR;
  // If rendering is off, the universal background color can be changed
  // using the current vram address.
  if (ppu_is_disabled() && ((ppu->vram_addr & VRAM_BUS_MASK)
                                                > PALETTE_BASE_ADDR)) {
    color_addr = ppu->vram_addr & VRAM_BUS_MASK;
  }

  // Get the background color pixel.
  word_t pixel = memory_vram_read(color_addr);
  word_t bg_pixel = 0;
  word_t sprite_pixel = 0;

  // Get the background tile pixel.
  if (ppu->mask & FLAG_RENDER_BG) {
    bg_pixel = ppu_render_get_tile_pixel();
    if (bg_pixel) { pixel = bg_pixel; }
  }

  // Get the sprite pixel.
  word_t sprite_index = ppu_render_get_sprite_index();
  if ((ppu->mask & FLAG_RENDER_SPRITES) && (sprite_index < ppu->sprite_count)) {
    sprite_pixel = ppu_render_get_sprite_pixel();
    word_t sprite_attribute = ppu->sprite_memory[4 * sprite_index + 2];

    // Check if the pixel should be rendered on top of the background.
    if ((sprite_pixel != 0) && ((bg_pixel == 0)
                            || !(sprite_attribute & FLAG_SPRITE_PRIORITY))) {
      pixel = sprite_pixel;
    }

    // Check if this counts as a sprite 0 hit.
    if ((sprite_index == 0) && (ppu->zero_in_mem) && (bg_pixel != 0)
                      && ((screen_x >= 8) || ((ppu->mask & FLAG_SHOW_BG)
                      && (ppu->mask & FLAG_SHOW_SPRITES)))
                      && (screen_x != 255) && (sprite_pixel != 0)
                      && (ppu->mask & FLAG_RENDER_BG)
                      && (ppu->mask & FLAG_RENDER_SPRITES)) {
      ppu->status |= FLAG_HIT;
    }
  }

  // Apply a greyscale effect to the pixel, if needed.
  if (ppu->mask & FLAG_GREYSCALE) { pixel &= GREYSCALE_PIXEL_MASK; }

  // Render the pixel.
  render_pixel(screen_y, screen_x, pixel);

  return;
}

/*
 * Pulls the next background tile pixel from the shift registers.
 *
 * Assumes the PPU has been initialized.
 */
word_t ppu_render_get_tile_pixel(void) {
  // Get the pattern of the background tile.
  word_t tile_pattern = (((ppu->tile_scroll[0] << ppu->fine_x) >> 7U) & 1U)
                      | (((ppu->tile_scroll[1] << ppu->fine_x) >> 6U) & 2U);

  // Determine if the background tile pixel is transparent, and load the color
  // if its not.
  if (tile_pattern) {
    // Get the palette of the background tile.
    word_t tile_palette = (((ppu->palette_scroll[0] << ppu->fine_x) >> 7U) & 1U)
                      | (((ppu->palette_scroll[1] << ppu->fine_x) >> 6U) & 2U);

    // Get the address of the background tile color and read in the pixel.
    dword_t tile_address = PALETTE_BASE_ADDR
                         | (tile_palette << 2U) | tile_pattern;
    return memory_vram_read(tile_address);
  } else {
    return 0x00U;
  }
}

/*
 * Pulls the next sprite pixel from the shift registers.
 *
 * Assumes the PPU has been initialized.
 * Assumes there is a visible sprite on the current pixel.
 */
word_t ppu_render_get_sprite_pixel(void) {
  // Determine which pixel of the sprite is to be rendered from its x position.
  size_t screen_x = current_cycle - 1;
  word_t sprite_index = ppu_render_get_sprite_index();
  word_t sprite_x = ppu->sprite_memory[(sprite_index << 2) + 3];
  word_t sprite_dx = screen_x - sprite_x;

  // Get the sprite pattern.
  word_t sprite_pattern = (((ppu->sprite_data[sprite_index][0]
                        << sprite_dx) >> 7U) & 1U)
                        | (((ppu->sprite_data[sprite_index][1]
                        << sprite_dx) >> 6U) & 2U);

  // Check if the pixel was transparent.
  if (sprite_pattern) {
    word_t sprite_palette = ppu->sprite_memory[(sprite_index << 2U) + 2U]
                          & FLAG_SPRITE_PALETTE;
    dword_t sprite_address = PALETTE_BASE_ADDR | SPRITE_PALETTE_BASE
                            | (sprite_palette << 2U) | sprite_pattern;
    return memory_vram_read(sprite_address);
  }

  // Since the sprite pixel was transparent, we return an empty pixel.
  return 0x00U;
}

/*
 * Gets the index of the first sprite in sprite memory for the given pixel.
 * Returns the value of ppu->sprite_count when no sprite is found.
 *
 * Assumes the PPU has been initialized and is drawing a visible pixel.
 */
word_t ppu_render_get_sprite_index(void) {
  // Find the first visible sprite.
  dword_t sprite_index = 0;
  dword_t screen_x = current_cycle - 1;
  for (; sprite_index < ppu->sprite_count; sprite_index++) {
    dword_t sprite_x = ppu->sprite_memory[4 * sprite_index + 3];
    if ((sprite_x <= screen_x) && ((sprite_x + 8) > screen_x)) { break; }
  }
  return sprite_index;
}

/*
 * Updates the background tile data shift registers based on the current cycle.
 * It takes 8 cycles to fully load in an 8x1 region for 1 tile.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_update_registers(void) {
  // Shift the tile registers.
  ppu->tile_scroll[0] = (ppu->tile_scroll[0] << 1) | (ppu->queued_bits[0] >> 7);
  ppu->queued_bits[0] = ppu->queued_bits[0] << 1;
  ppu->tile_scroll[1] = (ppu->tile_scroll[1] << 1) | (ppu->queued_bits[1] >> 7);
  ppu->queued_bits[1] = ppu->queued_bits[1] << 1;

  // Shift the pattern registers.
  ppu->palette_scroll[0] = (ppu->palette_scroll[0] << 1)
                         | (ppu->palette_latch[0]);
  ppu->palette_scroll[1] = (ppu->palette_scroll[1] << 1)
                         | (ppu->palette_latch[1]);

  // Reload the queued bits if the queue should now be empty.
  if ((current_cycle % 8) == 0) {
    ppu->queued_bits[0] = ppu->next_tile[0];
    ppu->queued_bits[1] = ppu->next_tile[1];
    ppu->palette_latch[0] = ppu->next_palette[0];
    ppu->palette_latch[1] = ppu->next_palette[1];
  }

  // Determine which of the 8 cycles is being executed.
  switch (current_cycle & REG_UPDATE_MASK) {
    case REG_FETCH_NT:
      ppu->mdr = memory_vram_read((ppu->vram_addr & VRAM_NT_ADDR_MASK)
                                                  | PPU_NT_OFFSET);
      break;
    case REG_FETCH_AT:
      ppu_render_get_attribute();
      break;
    case REG_FETCH_TILE_LOW:
      ppu->next_tile[0] = ppu_render_get_tile(ppu->mdr, false);
      break;
    case REG_FETCH_TILE_HIGH:
      ppu->next_tile[1] = ppu_render_get_tile(ppu->mdr, true);
      break;
    default:
      break;
  }

  return;
}

/*
 * Uses the current vram address and cycle/scanline position to load
 * attribute palette bits from the nametable into the next_palette variable.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_get_attribute(void) {
  // Get the coarse x and y from vram.
  dword_t coarse_x = ppu->vram_addr & COARSE_X_MASK;
  dword_t coarse_y = (ppu->vram_addr & COARSE_Y_MASK) >> 5;

  // Use the screen position to calculate the attribute table offset.
  dword_t attribute_offset = (coarse_x >> 2) | ((coarse_y >> 2) << 3);

  // Use the offset to calculate the address of the attribute table byte.
  dword_t attribute_addr = ATTRIBUTE_BASE_ADDR | attribute_offset
                         | (ppu->vram_addr & SCROLL_NT_MASK);
  word_t attribute = memory_vram_read(attribute_addr);

  // Isolate the color bits for the current quadrent the screen is drawing to.
  word_t attribute_x_shift = (coarse_x & 2U) ? 2U : 0U;
  word_t attribute_y_shift = (coarse_y & 2U) ? 4U : 0U;
  attribute = attribute >> (attribute_x_shift + attribute_y_shift);

  // Update the color palette bits using the attribute.
  ppu->next_palette[0] = attribute & 1U;
  ppu->next_palette[1] = (attribute >> 1U) & 1U;

  return;
}

/*
 * Uses the given index, and plane high toggle, to calculate the pattern table
 * address of a tile from its nametable byte. Returns the byte at said pattern
 * table address.
 *
 * Assumes the PPU has been initialized.
 */
word_t ppu_render_get_tile(word_t index, bool plane_high) {
  // Get the value of fine Y, to be used as a tile offset.
  dword_t tile_offset = (ppu->vram_addr & FINE_Y_MASK) >> FINE_Y_SHIFT;

  // Use the plane toggle to get the plane bit in the vram address.
  dword_t tile_plane = (plane_high) ? 0x08U : 0x00U;

  // Get the index in the vram address from the provided index.
  dword_t tile_index = ((dword_t) index) << 4U;

  // Get the side of the pattern table to be used from the control register.
  dword_t tile_table = (ppu->ctrl & FLAG_BG_TABLE) ? PATTERN_TABLE_HIGH
                                                   : PATTERN_TABLE_LOW;

  // Calculate the vram address of the tile byte and return the tile byte.
  dword_t tile_address = tile_table | tile_index | tile_plane | tile_offset;
  return memory_vram_read(tile_address);
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
    // Determine which sprite's pattern is being fetched.
    word_t sprite_index = (current_cycle - 257) / 8;
    word_t pattern_plane = ((current_cycle - 1) >> 1) & 1;
    CONTRACT(sprite_index < 8);

    // Check if we're on the first or second cycle of the read.
    if (ppu->mdr_write) {
      // Write to sprite data. If there are no more sprites, a transparent
      // pattern is read (0).
      ppu->sprite_data[sprite_index][pattern_plane] = ppu->mdr;
      ppu->mdr_write = false;
    } else {
      // Read the tile from the pattern table into the mdr.
      ppu->mdr = ppu_render_get_sprite();
      ppu->mdr_write = true;
    }
  }

  return;
}

/*
 * Fetches the pattern byte for a sprite on the next scanline.
 *
 * Assumes the ppu has been initialized and is in the sprite preparation
 * phase of a visible scanline.
 */
word_t ppu_render_get_sprite(void) {
  // Get some basic information about the current sprite being prepared.
  word_t sprite_index = (current_cycle - 257) / 8;
  word_t sprite_y = ppu->sprite_memory[4 * sprite_index];

  // If the data read did not actually belong to a sprite, we return an empty
  // pattern.
  if (sprite_index >= ppu->sprite_count) { return 0; }

  // Calculate position and offset from the sprite.
  dword_t tile_index = ppu->sprite_memory[4 * sprite_index + 1];
  // If the sprite is 8x16, and the bottom half is being rendered,
  // we need to move to the next tile. An offset is calculated to do this.
  dword_t index_offset = 0;
  if ((ppu->ctrl & FLAG_SPRITE_SIZE) && (current_scanline >= (sprite_y + 8U))) {
    index_offset = X16_INDEX_OFFSET;
    sprite_y += 8;
  }
  // This tile offset determines which of the 8 rows of the tile will be
  // returned.
  dword_t tile_offset = current_scanline - sprite_y; // TODO: Off by one?
  CONTRACT(tile_offset < 8);
  // Each tile has two planes, which are used to denote its color in a palette.
  dword_t tile_plane = ((current_cycle - 1) >> 1) & 1;

  // Check if the sprite is being flipped vertically.
  if (ppu->sprite_memory[4 * sprite_index + 2] & FLAG_SPRITE_VFLIP) {
    tile_offset = 7 - tile_offset;
    if (ppu->ctrl & FLAG_SPRITE_SIZE) { index_offset ^= X16_INDEX_OFFSET; }
  }

  // Determine which size of sprites are being used and then
  // calculate the pattern address.
  dword_t tile_pattern;
  if (ppu->ctrl & FLAG_SPRITE_SIZE) {
    tile_pattern = tile_offset | (tile_plane << X16_PLANE_SHIFT)
                 | ((tile_index & X16_TILE_MASK) << X16_TILE_SHIFT)
                 | ((tile_index & X16_TABLE_MASK) << X16_TABLE_SHIFT);
    tile_pattern += index_offset;
  } else {
    tile_pattern = tile_offset | (tile_plane << X8_PLANE_SHIFT)
                 | (tile_index << X8_TILE_SHIFT)
                 | ((ppu->ctrl & FLAG_SPRITE_TABLE) << X8_TABLE_SHIFT);
  }

  // Use the calculated pattern address to get the tile byte.
  word_t tile = memory_vram_read(tile_pattern);

  // Check if the byte should be horizontally flipped.
  if (ppu->sprite_memory[4 * sprite_index + 2] & FLAG_SPRITE_HFLIP) {
    tile = reverse_word(tile);
  }

  return tile;
}

/*
 * Executes 2 dummy nametable fetches over 4 cycles.
 *
 * Assumes the PPU has been initialized.
 */
void ppu_render_dummy_nametable_access(void) {
  // Determine which cycle of the fetch we are on.
  if (ppu->mdr_write) {
    // Second cycle, thrown away internally.
    ppu->mdr_write = false;
  } else {
    // First cycle, reads from vram.
    ppu->mdr = memory_vram_read((ppu->vram_addr & VRAM_NT_ADDR_MASK)
                                                | PPU_NT_OFFSET);
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
    ppu->vram_addr &= ~SCROLL_Y_MASK;
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
  // The status flags are reset at the begining of the pre-render scanline.
  if (current_cycle == 1) { ppu->status = 0; }

  // Determine which phase of rendering the scanline is in.
  if (current_cycle > 0 && current_cycle <= 256) {
    // Accesses are made, but nothing is rendered.
    ppu_render_update_frame(false);
  } else if (current_cycle == 257) {
    ppu_render_update_hori();
  } else if (current_cycle >= 280 && current_cycle <= 304) {
    // Update the vertical part of the vram address register.
    ppu_render_update_vert();
  } else if (current_cycle > 320 && current_cycle <= 336) {
    // Fetch the background tile data for the next cycle.
    ppu_render_update_registers();
    if ((current_cycle % 8) == 0) { ppu_render_xinc(); }
  } else if (current_cycle > 336 && current_cycle <= 340) {
    // Unused NT byte fetches, mappers may clock this.
    ppu_render_dummy_nametable_access();
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
    ppu->soam_addr = (current_cycle >> 1) & 0x1F;
    ppu->secondary_oam[ppu->soam_addr] = 0xFF;
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
    ppu->zero_index = ppu->oam_addr;
    ppu->zero_in_soam = false;
    ppu->next_sprite_count = 0;
  }

  // On odd cycles, data is read from primary OAM.
  if (current_cycle & 1U) {
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
        // If this is sprite zero, we mark it as in soam.
        if (ppu->oam_addr == ppu->zero_index) { ppu->zero_in_soam = true; }
        // Increment and prepare to copy the sprite data to secondary OAM.
        ppu->eval_state = COPY_TILE;
        ppu->oam_addr++;
        ppu->soam_addr++;
        ppu->next_sprite_count++;
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
 * Performs a write to secondary OAM during sprite evaluation.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_eval_write_soam(void) {
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
                      && (current_scanline < (sprite_y + sprite_size));
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
  // Flags if sprite zero is in slot 0.
  if (current_cycle == 257) {
    ppu->soam_addr = 0;
    ppu->zero_in_mem = ppu->zero_in_soam;
    ppu->sprite_count = ppu->next_sprite_count;
  }

  // Sprite evaluation and rendering alternate access to sprite data every 4
  // cycles between 257 and 320.
  if (((current_cycle - 1) & 0x04U) == 0) {
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
          && frame_odd && (current_scanline >= 261)
          && ((ppu->mask & FLAG_RENDER_BG)
          || (ppu->mask & FLAG_RENDER_SPRITES)))) {
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
      ppu_oam_dma(val);
      break;
    case PPU_SCROLL_ACCESS:
      ppu_mmio_scroll_write(val);
      break;
    case PPU_ADDR_ACCESS:
      ppu_mmio_addr_write(val);
      break;
    case PPU_DATA_ACCESS:
      // Writes can only happen during vblank.
      memory_vram_write(val, ppu->vram_addr & VRAM_BUS_MASK);
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
    // Update scroll Y.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & SCROLL_X_MASK)
             | (ppu->temp_vram_addr & SCROLL_NT_MASK)
             | ((((dword_t) val) << COARSE_Y_SHIFT) & COARSE_Y_MASK)
             | ((((dword_t) val) << FINE_Y_SHIFT) & FINE_Y_MASK);
  } else {
    // Update scroll X.
    ppu->fine_x = val & FINE_X_MASK;
    ppu->temp_vram_addr = (ppu->temp_vram_addr & SCROLL_Y_MASK)
                        | (ppu->temp_vram_addr & SCROLL_NT_MASK)
                        | (val >> COARSE_X_SHIFT);
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
  if (ppu_is_disabled() || (current_scanline >= 240
                            && current_scanline <= 260)) {
    // When the PPU is inactive, vram is incremented correctly.
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
      ppu->bus = ppu_oam_read();
      break;
    case PPU_DATA_ACCESS:
      // Reading from mappable VRAM (not the palette) returns the value to an
      // internal bus.
      if (reg_addr < PPU_PALETTE_OFFSET) {
        ppu->bus = ppu->vram_bus;
        ppu->vram_bus = memory_vram_read(reg_addr);
      } else {
        ppu->bus = memory_vram_read(reg_addr);
        ppu->vram_bus = memory_vram_read((reg_addr & VRAM_NT_ADDR_MASK)
                                                   | PPU_NT_OFFSET);
      }
      ppu_mmio_vram_addr_inc();
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
  if (ppu_is_disabled() || (current_scanline >= 240
                        && current_scanline <= 260)) {
    ppu->primary_oam[ppu->oam_addr] = val;
    ppu->oam_addr++;
  } else {
    // Writes during rendering cause a bad increment and ignore the write.
    ppu->oam_addr += 4;
  }
  return;
}

/*
 * Reads a byte from primary OAM, accounting for the internal signal which may
 * force the value to 0xFF.
 *
 * Assumes the ppu has been initialized.
 */
word_t ppu_oam_read(void) {
  return ppu->primary_oam[ppu->oam_addr] | ppu->oam_mask;
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
