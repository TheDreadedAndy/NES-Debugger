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
#define SOAM_BUFFER_SIZE 256U
#define NUM_SOAM_BUFFERS 2U
#define OAM_BUFFER_SIZE 96U

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
#define FLAG_LEFT_SPRITES 0x04U
#define FLAG_LEFT_BG 0x02U
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
#define SPRITE_PLANE_HIGH_MASK 0x08U
#define X16_INDEX_OFFSET 0x10U
#define X16_TILE_MASK 0xFEU
#define X16_TILE_SHIFT 4
#define X16_TABLE_MASK 0x01U
#define X16_TABLE_SHIFT 12
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
 * Flags used to access the SOAM buffer for sprites.
 */
#define FLAG_SOAM_BUFFER_ZERO 0x80U
#define FLAG_SOAM_BUFFER_PRIORITY 0x40U
#define FLAG_SOAM_BUFFER_PALETTE 0x1FU
#define FLAG_SOAM_BUFFER_PATTERN 0x03U

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
  word_t vram_buf;
  word_t ctrl;
  word_t mask;
  word_t status;
  word_t oam_addr;

  // Working memory for the ppu.
  word_t oam_mask;
  word_t primary_oam[PRIMARY_OAM_SIZE];
  word_t soam_eval_buf;
  word_t soam_render_buf;
  word_t soam_buffer[NUM_SOAM_BUFFERS][SOAM_BUFFER_SIZE];
  word_t oam_buffer[OAM_BUFFER_SIZE];

  // Temporary storage used in rendering.
  mword_t tile_scroll[BIT_PLANES];
  word_t next_tile[BIT_PLANES];
  word_t palette_scroll[BIT_PLANES];
  word_t palette_latch[BIT_PLANES];
  word_t next_palette[BIT_PLANES];

  // MDR and write toggle, used for 2-cycle r/w system.
  word_t mdr;
  bool mdr_write;
} ppu_t;

/*
 * The actions performed by the ppu each cycle depend entirely on which cycle
 * and scanline it is on. These variables track that information so that the
 * ppu emulation can adjust acordingly.
 */
static size_t current_scanline = 261;
static size_t current_cycle = 0;
static bool frame_odd = false;

/*
 * Global ppu structure. Cannot be accessed outside this file.
 * Initialized by ppu_init().
 */
static ppu_t *ppu = NULL;

/* Helper functions */
static bool ppu_is_disabled(void);
static void ppu_disabled(void);
static void ppu_draw_background(void);
static void ppu_render(void);
static void ppu_render_visible(void);
static void ppu_render_update_frame(bool output);
static void ppu_render_draw_pixel(void);
static word_t ppu_render_get_tile_pattern(void);
static word_t ppu_render_get_tile_palette(void);
static void ppu_render_update_registers(void);
static void ppu_render_get_attribute(void);
static word_t ppu_render_get_tile(word_t index, bool plane_high);
static void ppu_render_update_hori(void);
static void ppu_render_dummy_nametable_access(void);
static void ppu_render_xinc(void);
static void ppu_render_yinc(void);
static void ppu_render_blank(void);
static void ppu_render_pre(void);
static void ppu_render_update_vert(void);
static void ppu_eval_clear_soam(void);
static void ppu_eval_sprites(void);
static word_t ppu_oam_read(void);
static bool ppu_eval_in_range(word_t sprite_y);
static void ppu_eval_fill_soam_buffer(word_t *sprite_data, bool is_zero);
static void ppu_eval_get_sprite(word_t *sprite_data, word_t *pat_lo,
                                                     word_t *pat_hi);
static void ppu_eval_fetch_sprites(void);
static void ppu_signal(void);
static void ppu_inc(void);
static void ppu_mmio_scroll_write(word_t val);
static void ppu_mmio_addr_write(word_t val);
static void ppu_mmio_vram_addr_inc(void);

/*
 * Initializes the PPU and palette, using the given file, then creates an
 * SDL window.
 *
 * Assumes the file is non-NULL.
 */
bool ppu_init(char *file) {
  // Prepare the ppu structure.
  ppu = xcalloc(1, sizeof(ppu_t));
  ppu->soam_eval_buf = 1;

  // Load in the palette.
  palette_init(file);

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
static bool ppu_is_disabled(void) {
  return !(ppu->mask & FLAG_RENDER_BG) && !(ppu->mask & FLAG_RENDER_SPRITES);
}

/*
 * Performs any PPU actions which cannot be disabled.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_disabled(void) {
  // The OAM mask should not be active when rendering is disabled.
  ppu->oam_mask = 0;

  // Determine which action should be performed.
  if ((current_scanline >= 8) && (current_scanline < 232)) {
    // Draws the background color to the screen when rendering is disabled.
    if ((current_cycle > 0) && (current_cycle < 257)) { ppu_draw_background(); }
  } else if ((current_scanline >= 240) && (current_scanline < 261)) {
    // Signals the start of vblanks.
    ppu_render_blank();
  } else if ((current_scanline >= 261) && (current_cycle == 1)) {
    // Resets the status register during the pre-render scanline.
    ppu->status = 0;
  }

  return;
}

/*
 * Draws the background color to the screen when rendering is disabled.
 *
 * Assumes the PPU has been initialized and rendering is disabled.
 */
static void ppu_draw_background(void) {
  // Get the universal background color address and the screen position.
  size_t screen_x = current_cycle - 1;
  size_t screen_y = current_scanline;
  dword_t color_addr = PALETTE_BASE_ADDR;

  // The universal background color can be changed using the current vram
  // address.
  if ((ppu->vram_addr & VRAM_BUS_MASK) > PALETTE_BASE_ADDR) {
    color_addr = ppu->vram_addr & VRAM_BUS_MASK;
  }

  // Get the background color pixel.
  uint32_t pixel = memory_palette_read(color_addr);

  // Render the pixel.
  render->pixel(screen_y, screen_x, pixel);

  return;
}


/*
 * Runs the rendering action for the given cycle/scanline.
 *
 * Assumes the ppu has been initialized.
 */
static void ppu_render(void) {
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
static void ppu_render_visible(void) {
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
    // address register. As an optmization, sprite fetching is run only on this
    // cycle.
    if (current_cycle == 257) {
      ppu_render_update_hori();
      ppu_eval_fetch_sprites();
    }
    // The OAM addr is reset to zero during sprite prep, which has been
    // optimized out.
    ppu->oam_addr = 0;
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
static void ppu_render_update_frame(bool output) {
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
static void ppu_render_draw_pixel(void) {
  // Get the screen position.
  size_t screen_x = current_cycle - 1;
  size_t screen_y = current_scanline;

  // Holds the background and sprite palette color index.
  word_t bg_pattern = 0;

  // Flags whether the background or sprite pixel should be rendered.
  bool sprite_on_top = false;

  // Get the background tile pattern.
  if ((ppu->mask & FLAG_RENDER_BG) && ((screen_x >= 8)
                                   || (ppu->mask & FLAG_LEFT_BG))) {
    bg_pattern = ppu_render_get_tile_pattern();
  }

  // Get the sprite pattern.
  word_t sprite_buf = 0xFF;
  if ((ppu->mask & FLAG_RENDER_SPRITES)
     && ((screen_x >= 8) || (ppu->mask & FLAG_LEFT_SPRITES))
     && ((sprite_buf = ppu->soam_buffer[ppu->soam_render_buf][screen_x])
     != 0xFF)) {

    // Check if the pixel should be rendered on top of the background.
    if (((bg_pattern == 0) || !(sprite_buf & FLAG_SOAM_BUFFER_PRIORITY))
                           && (sprite_buf & FLAG_SOAM_BUFFER_PATTERN)) {
      sprite_on_top = true;
    }

    // Check if this counts as a sprite 0 hit.
    if ((sprite_buf & FLAG_SOAM_BUFFER_ZERO) && (bg_pattern != 0)
         && (screen_x != 255) && (sprite_buf & FLAG_SOAM_BUFFER_PATTERN)) {
      ppu->status |= FLAG_HIT;
    }
  }

  // Calculate the address of the pixel to be drawn.
  dword_t pixel_addr;
  if (sprite_on_top) {
    pixel_addr = PALETTE_BASE_ADDR | (sprite_buf & FLAG_SOAM_BUFFER_PALETTE);
  } else if (bg_pattern != 0) {
    pixel_addr = PALETTE_BASE_ADDR | bg_pattern
               | ppu_render_get_tile_palette();
  } else {
    pixel_addr = PALETTE_BASE_ADDR;
  }

  // Check if the pixel is on a scanline that is displayed.
  if ((current_scanline >= 8) && (current_scanline < 232)) {
    // Get the pixel.
    uint32_t pixel = memory_palette_read(pixel_addr);

    // Render the pixel.
    render->pixel(screen_y, screen_x, pixel);
  }

  return;
}

/*
 * Pulls the next background tile pattern from the shift registers.
 *
 * Assumes the PPU has been initialized.
 */
static word_t ppu_render_get_tile_pattern(void) {
  // Get the pattern of the background tile.
  word_t tile_pattern = (((ppu->tile_scroll[0].w[WORD_HI] << ppu->fine_x)
                      >> 7U) & 1U)
                      | (((ppu->tile_scroll[1].w[WORD_HI] << ppu->fine_x)
                      >> 6U) & 2U);
  return tile_pattern;
}

/*
 * Gets the palette index of the current background tile from the shift
 * registers.
 *
 * Assumes the PPU has been initialized.
 * Assumes the current tile pattern is non-zero.
 */
static word_t ppu_render_get_tile_palette(void) {
  // Get the palette of the background tile.
  word_t tile_palette = (((ppu->palette_scroll[0] << ppu->fine_x) >> 7U) & 1U)
                      | (((ppu->palette_scroll[1] << ppu->fine_x) >> 6U) & 2U);
  return tile_palette << 2U;
}

/*
 * Updates the background tile data shift registers based on the current cycle.
 * It takes 8 cycles to fully load in an 8x1 region for 1 tile.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_render_update_registers(void) {
  // Shift the tile registers.
  ppu->tile_scroll[0].dw = ppu->tile_scroll[0].dw << 1;
  ppu->tile_scroll[1].dw = ppu->tile_scroll[1].dw << 1;

  // Shift the pattern registers.
  ppu->palette_scroll[0] = (ppu->palette_scroll[0] << 1)
                         | (ppu->palette_latch[0]);
  ppu->palette_scroll[1] = (ppu->palette_scroll[1] << 1)
                         | (ppu->palette_latch[1]);

  // Reload the queued bits if the queue should now be empty.
  if ((current_cycle % 8) == 0) {
    ppu->tile_scroll[0].w[WORD_LO] = ppu->next_tile[0];
    ppu->tile_scroll[1].w[WORD_LO] = ppu->next_tile[1];
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
static void ppu_render_get_attribute(void) {
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
static word_t ppu_render_get_tile(word_t index, bool plane_high) {
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
static void ppu_render_update_hori(void) {
  // Copies the coarse X and horizontal nametable bit from t to v.
  ppu->vram_addr = (ppu->vram_addr & (SCROLL_Y_MASK | SCROLL_VNT_MASK))
                 | (ppu->temp_vram_addr & (SCROLL_X_MASK | SCROLL_HNT_MASK));
  return;
}

/*
 * Executes 2 dummy nametable fetches over 4 cycles.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_render_dummy_nametable_access(void) {
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
static void ppu_render_xinc(void) {
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
static void ppu_render_yinc(void) {
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
static void ppu_render_blank(void) {
  if (current_scanline == 241 && current_cycle == 1) {
    // TODO: Implement special case timing.
    ppu->status |= FLAG_VBLANK;
    render->frame();
  }
  return;
}

/*
 * Execute the pre-render scanline acording to the current cycle.
 * Resets to the status flags are performed here.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_render_pre(void) {
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
static void ppu_render_update_vert(void) {
  // Copies the fine Y, coarse Y, and vertical nametable bit from t to v.
  ppu->vram_addr = (ppu->vram_addr & (SCROLL_X_MASK | SCROLL_HNT_MASK))
                 | (ppu->temp_vram_addr & (SCROLL_Y_MASK | SCROLL_VNT_MASK));
  return;
}

/*
 * Sets every value in secondary OAM to 0xFF. Used in sprite evaluation.
 *
 * Assumes the ppu has been initialized.
 */
static void ppu_eval_clear_soam(void) {
  // An internal signal which makes all primary OAM reads return 0xFF should
  // be raised during this phase of sprite evaluation.
  ppu->oam_mask = 0xFF;

  // In the real PPU, SOAM is cleared by writing 0xFF every even cycle.
  // It is more cache efficient, however, to do it all at once here.
  if (current_cycle == 1) {
    for (size_t i = 0; i < SOAM_BUFFER_SIZE; i++) {
      ppu->soam_buffer[ppu->soam_eval_buf][i] = 0xFF;
    }
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
static void ppu_eval_sprites(void) {
  // As an optimization, sprite evaluation is run only once per scanline.
  if (current_cycle != 65) { return; }

  // Run sprite evaluation. This loop fills both the OAM read buffer,
  // which returns the correct values read from OAM each cycle on request
  // from the CPU. Additionally, this loop fills the SOAM scanline buffer,
  // which contains one byte of sprite data for each pixel to be rendered.
  size_t i = ppu->oam_addr;
  size_t sim_cycle = 0;
  size_t sprites_found = 0;
  while (i < PRIMARY_OAM_SIZE) {
    // Read in the sprite and check if it was in range.
    ppu->oam_buffer[sim_cycle] = ppu->primary_oam[i];
    if (sprites_found >= 8) {
      // More than 8 sprites have been found, so spirte overflow begins.
      if (ppu_eval_in_range(ppu->oam_buffer[sim_cycle])) {
        // Flag that overflow occured, and fill the rest of the OAM buffer.
        // FIXME: Should not be set immediately.
        ppu->status |= FLAG_OVERFLOW;
        for (; sim_cycle < OAM_BUFFER_SIZE; sim_cycle++) {
          ppu->oam_buffer[sim_cycle] = ppu->primary_oam[i];
        }
        break;
      } else {
        // Sprite overflow is bugged, and does not increment correctly.
        sim_cycle++;
        i += 5;
      }
    } else if (ppu_eval_in_range(ppu->oam_buffer[sim_cycle])) {
      // If it was, read it into the OAM buffer and add it to the scanline
      // buffer.
      sprites_found++;
      for (int j = 0; j < 4; j++) {
        ppu->oam_buffer[sim_cycle] = ppu->primary_oam[i];
        i++;
      }
      ppu_eval_fill_soam_buffer(&(ppu->primary_oam[i - 4]),
                                 (i - 4) == ppu->oam_addr);
    } else {
      // Otherwise, the sprite was out of range, so we move on.
      i += 4;
      sim_cycle++;
    }
  }
  ppu->oam_addr = i;

  return;
}

/*
 * Checks if a sprite is visible on the current scanline using the value
 * stored in the eval buffer as a Y cordinate.
 *
 * Assumes the PPU has been initialized.
 */
static bool ppu_eval_in_range(word_t sprite_y) {
  // Get the current size of sprites (8x8 or 8x16) from the control register
  // and the screen y coordinate from the current scanline.
  word_t sprite_size = (ppu->ctrl & FLAG_SPRITE_SIZE) ? 16 : 8;
  word_t screen_y = current_scanline;

  // Check if the sprite is visible on this scanline.
  bool in_range = (sprite_y <= screen_y) && (sprite_y < 240)
                      && (screen_y < (sprite_y + sprite_size));
  return in_range;
}

/*
 * Adds the given sprite to the soam sprite buffer, setting its sprite zero
 * bit if is_zero is set.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_eval_fill_soam_buffer(word_t *sprite_data, bool is_zero) {
  // Setup the information that will be contained in each scanline buffer byte
  // for this sprite. This byte contains whether or not the pixel belongs to
  // sprite zero, the sprites priority, and its palette index.
  word_t base_byte = SPRITE_PALETTE_BASE;
  if (is_zero) { base_byte |= FLAG_SOAM_BUFFER_ZERO; }
  if (sprite_data[2] & FLAG_SPRITE_PRIORITY) {
    base_byte |= FLAG_SOAM_BUFFER_PRIORITY;
  }
  base_byte |= (sprite_data[2] & FLAG_SPRITE_PALETTE) << 2;

  // Get the individual pixels for the sprite.
  word_t pat_lo, pat_hi;
  ppu_eval_get_sprite(sprite_data, &pat_lo, &pat_hi);

  // Add the sprite to the soam buffer.
  word_t sprite_x = sprite_data[3];
  for (size_t i = sprite_x; (i < sprite_x + 8U)
                         && (i < SOAM_BUFFER_SIZE); i++) {
    // The sprite pixel should only be added to the buffer if no other sprite
    // has been rendered to that pixel.
    word_t buf_byte = ppu->soam_buffer[ppu->soam_eval_buf][i];
    if ((buf_byte == 0xFF) || !(buf_byte & FLAG_SOAM_BUFFER_PATTERN))  {
      ppu->soam_buffer[ppu->soam_eval_buf][i] = base_byte
                                              | ((pat_hi >> 6U) & 2U)
                                              | ((pat_lo >> 7U) & 1U);
    }

    // Shift the data to the next sprite pixel.
    pat_lo <<= 1U;
    pat_hi <<= 1U;
  }

  return;
}

/*
 * Fetches the pattern bytes for a given sprite.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_eval_get_sprite(word_t *sprite_data, word_t *pat_lo,
                                                     word_t *pat_hi) {
  // Get some basic information about the current sprite being prepared.
  word_t screen_y = current_scanline;
  word_t sprite_y = sprite_data[0];

  // Calculate position and offset from the sprite.
  dword_t tile_index = sprite_data[1];
  // If the sprite is 8x16, and the bottom half is being rendered,
  // we need to move to the next tile. An offset is calculated to do this.
  dword_t index_offset = 0;
  if ((ppu->ctrl & FLAG_SPRITE_SIZE) && (screen_y >= (sprite_y + 8U))) {
    index_offset = X16_INDEX_OFFSET;
    sprite_y += 8;
  }
  // This tile offset determines which of the 8 rows of the tile will be
  // returned.
  dword_t tile_offset = screen_y - sprite_y;
  CONTRACT(tile_offset < 8);

  // Check if the sprite is being flipped vertically.
  if (sprite_data[2] & FLAG_SPRITE_VFLIP) {
    tile_offset = (~tile_offset) & 0x07U;
    if (ppu->ctrl & FLAG_SPRITE_SIZE) { index_offset ^= X16_INDEX_OFFSET; }
  }

  // Determine which size of sprites are being used and then
  // calculate the pattern address.
  dword_t tile_addr;
  if (ppu->ctrl & FLAG_SPRITE_SIZE) {
    tile_addr = tile_offset
              | ((tile_index & X16_TILE_MASK) << X16_TILE_SHIFT)
              | ((tile_index & X16_TABLE_MASK) << X16_TABLE_SHIFT)
              | index_offset;
  } else {
    dword_t tile_table = (ppu->ctrl & FLAG_SPRITE_TABLE) ? PATTERN_TABLE_HIGH
                                                         : PATTERN_TABLE_LOW;
    tile_addr = tile_offset | (tile_index << X8_TILE_SHIFT) | tile_table;
  }

  // Use the calculated pattern address to get the tile bytes.
  *pat_lo = memory_vram_read(tile_addr);
  *pat_hi = memory_vram_read(tile_addr | SPRITE_PLANE_HIGH_MASK);

  // Check if the bytes should be horizontally flipped.
  if (sprite_data[2] & FLAG_SPRITE_HFLIP) {
    *pat_lo = reverse_word(*pat_lo);
    *pat_hi = reverse_word(*pat_hi);
  }

  return;
}

/*
 * Switches the active sprite scanline buffers once per scanline.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_eval_fetch_sprites(void) {
  if (current_cycle == 257) {
    // In place switch.
    ppu->soam_eval_buf ^= ppu->soam_render_buf;
    ppu->soam_render_buf ^= ppu->soam_eval_buf;
    ppu->soam_eval_buf ^= ppu->soam_render_buf;
  }

  return;
}

/*
 * Generates an NMI in the CPU emulation when appropriate.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_signal(void) {
  // NMIs should be generated when they are enabled in ppuctrl and
  // the ppu is in vblank.
  nmi_line = (ppu->ctrl & FLAG_ENABLE_VBLANK) && (ppu->status & FLAG_VBLANK);
  return;
}

/*
 * Increments the scanline, cycle, and frame type and correctly wraps them.
 * Each ppu frame has 341 cycles and 262 scanlines.
 */
static void ppu_inc(void) {
  // Increment the cycle.
  current_cycle++;

  // Increment the scanline if it is time to wrap the cycle.
  if ((current_cycle > 340) || (frame_odd
          && (current_cycle > 339) && (current_scanline >= 261)
          && ((ppu->mask & FLAG_RENDER_BG)
          || (ppu->mask & FLAG_RENDER_SPRITES)))) {
    current_scanline++;
    current_cycle = 0;

    // Wrap the scanline and toggle the frame if it is time to do so.
    if (current_scanline > 261) {
      current_scanline = 0;
      frame_odd = !frame_odd;
    }
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
      palette_update_mask(val);
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
static void ppu_mmio_scroll_write(word_t val) {
  // Determine which write should be done.
  if (ppu->write_toggle) {
    // Update scroll Y.
    ppu->temp_vram_addr = (ppu->temp_vram_addr
             & (SCROLL_X_MASK | SCROLL_NT_MASK))
             | ((((dword_t) val) << COARSE_Y_SHIFT) & COARSE_Y_MASK)
             | ((((dword_t) val) << FINE_Y_SHIFT) & FINE_Y_MASK);
    ppu->write_toggle = false;
  } else {
    // Update scroll X.
    ppu->fine_x = val & FINE_X_MASK;
    ppu->temp_vram_addr = (ppu->temp_vram_addr
                        & (SCROLL_Y_MASK | SCROLL_NT_MASK))
                        | (val >> COARSE_X_SHIFT);
    ppu->write_toggle = true;
  }

  return;
}

/*
 * Writes to the PPU addr register. Toggles the write bit.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_mmio_addr_write(word_t val) {
  // Determine which write should be done.
  if (ppu->write_toggle) {
    // Write the low byte and update v.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & PPU_ADDR_HIGH_MASK) | val;
    ppu->vram_addr = ppu->temp_vram_addr;
    ppu->write_toggle = false;
  } else {
    // Write the high byte.
    ppu->temp_vram_addr = (ppu->temp_vram_addr & PPU_ADDR_LOW_MASK)
        | ((((dword_t) val) << PPU_ADDR_HIGH_SHIFT) & PPU_ADDR_HIGH_MASK);
    ppu->write_toggle = true;
  }

  return;
}

/*
 * Increments the vram address after a PPU data access, implementing the
 * buggy behavior expected during rendering.
 *
 * Assumes the PPU has been initialized.
 */
static void ppu_mmio_vram_addr_inc(void) {
  // Determine how the increment should work.
  if (ppu_is_disabled() || (current_scanline >= 240
                            && current_scanline <= 260)) {
    // When the PPU is inactive, vram is incremented correctly.
    ppu->vram_addr = (ppu->ctrl & FLAG_VRAM_VINC) ? (ppu->vram_addr + 32)
                                                  : (ppu->vram_addr + 1);
  } else if (!(((current_cycle > 0 && current_cycle <= 256)
         || current_cycle > 320) && (current_cycle % 8) == 0)) {
    // Writing to PPU data during rendering causes a X and Y increment.
    // This only happens when the PPU would not otherwise be incrementing them.
    // FIXME: May not be both at once.
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
        ppu->bus = ppu->vram_buf;
        ppu->vram_buf = memory_vram_read(ppu->vram_addr);
      } else {
        ppu->bus = memory_vram_read(ppu->vram_addr);
        ppu->vram_buf = memory_vram_read((ppu->vram_addr & VRAM_NT_ADDR_MASK)
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
  // Writes during rendering are ignored.
  if (ppu_is_disabled() || (current_scanline >= 240
                        && current_scanline <= 260)) {
    ppu->primary_oam[ppu->oam_addr] = val;
    ppu->oam_addr++;
  }

  // The PPU bus is filled with the value, incase we are coming from a CPU DMA.
  ppu->bus = val;
  return;
}

/*
 * Reads a byte from primary OAM, accounting for the internal signal which may
 * force the value to 0xFF.
 *
 * Assumes the ppu has been initialized.
 */
static word_t ppu_oam_read(void) {
  // If the PPU is in the middle of sprite rendering, we return the buffered
  // value that would of been read on this cycle.
  if ((current_scanline < 240) && (current_cycle > 64)
                               && (current_cycle <= 256)) {
    return ppu->oam_buffer[(current_cycle - 1) >> 1];
  } else {
    // Otherwise, we return the value in OAM at the current address.
    return ppu->primary_oam[ppu->oam_addr] | ppu->oam_mask;
  }
}

/*
 * Frees the global ppu structure.
 *
 * Assumes the ppu has been initialized.
 */
void ppu_free(void) {
  // Free the ppu structure.
  free(ppu);

  // Free the NES palette data.
  palette_free();

  return;
}
