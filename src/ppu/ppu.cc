/*
 * The file contains the Ppu class and all of its member functions. The
 * Ppu class is used to emulation the NES's PPU, which renders tiles and
 * sprites to the screen.
 *
 * The CPU emulation can communicate with this class through memory, which
 * properly maps the PPU's MMIO addresses to the MMIO member functions of
 * the Ppu class.
 *
 * The NES PPU has many strange and counter-intuitive bugs, and so great
 * care should be taken when editing this file to ensure compatibility.
 * This is especially important when editing code related to sprite layering.
 *
 * More information on the specific opperations of the PPU is available
 * on nesdev.com.
 */

#include "./ppu.h"

#include <new>
#include <cstdlib>

#include "../util/data.h"
#include "../util/util.h"
#include "../util/contracts.h"
#include "../cpu/cpu.h"
#include "../sdl/renderer.h"
#include "../memory/memory.h"
#include "../memory/palette.h"

/* Emulation constants */

// Object Attribute Memory size.
#define PRIMARY_OAM_SIZE 256U
#define SOAM_BUFFER_SIZE 256U
#define OAM_BUFFER_SIZE 96U

// Mask for determining which register a mmio access is trying to use.
#define PPU_MMIO_MASK 0x0007U

// Flag masks for the PPU status register.
#define FLAG_VBLANK 0x80U
#define FLAG_HIT 0x40U
#define FLAG_OVERFLOW 0x20U

// PPU status is a 3 bit register.
#define PPU_STATUS_MASK 0xE0U

// Flag masks for the PPU mask register.
#define FLAG_RENDER_SPRITES 0x10U
#define FLAG_RENDER_BG 0x08U
#define FLAG_LEFT_SPRITES 0x04U
#define FLAG_LEFT_BG 0x02U

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

// Used, during rendering, to determine which piece of the tile should be
// loaded on a given cycle.
#define REG_UPDATE_MASK 0x07U
#define REG_APPLY_UPDATES 0U
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

// The lower three bits of an mmio access to the ppu determine which register
// is used.
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

// Accesses to PPU scroll adjust the PPU address in non-standard ways.
// These constants determine how to update the scroll value.
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

// The vram address of the ppu can be incremented based on the X and Y
// scroll values. These constants facilitate that.
#define FINE_Y_INC 0x1000U
#define FINE_Y_CARRY_MASK 0x8000U
#define FINE_Y_CARRY_SHIFT 10
#define COARSE_X_CARRY_MASK 0x0020U
#define Y_INC_OVERFLOW 0x03C0U
#define TOGGLE_HNT_SHIFT 5

// Flags used to access the SOAM buffer for sprites.
#define FLAG_SOAM_BUFFER_ZERO 0x80U
#define FLAG_SOAM_BUFFER_PRIORITY 0x40U
#define FLAG_SOAM_BUFFER_PALETTE 0x1FU
#define FLAG_SOAM_BUFFER_PATTERN 0x03U

/*
 * Initializes the PPU and palette, using the palette file specified in the
 * config if it exists.
 *
 * Assumes the provided configuration object is valid.
 */
Ppu::Ppu(void) {
  // Allocate the renderering buffers.
  primary_oam_ = new DataWord[PRIMARY_OAM_SIZE]();
  soam_buffer_[0] = new DataWord[SOAM_BUFFER_SIZE]();
  soam_buffer_[1] = new DataWord[SOAM_BUFFER_SIZE]();
  tile_buffer_ = new DataWord[kTileBufferSize_]();

  return;
}

/*
 * Connects the PPU to the rest of the emulated system. This function
 * must be called after initialization to put the PPU in a valid state.
 */
void Ppu::Connect(Memory *memory, Renderer *render, bool *nmi_line) {
  // Store the given objects.
  memory_ = memory;
  renderer_ = render;
  nmi_line_ = nmi_line;

  // Get a pointer to the palette data in memory.
  pixel_data_ = memory_->PaletteExpose();

  return;
}

/*
 * Determines how many cycles can be executed before the ppu will attempt
 * an action which will effect the state of another chip. This corresponds to
 * the number of cycles until the next positive NMI edge.
 *
 * If NMI's are disable, this function will return 64-bit unsigned int max.
 */
size_t Ppu::Schedule(void) {
  // Constants used to calculate the number of safe cycles.
  const size_t cycles_per_line = 341;
  const size_t lines_per_frame = 262;
  const size_t nmi_line = 241;
  const size_t nmi_cycle = 1;

  // Check if NMI's are disabled.
  if (!(ctrl_ & FLAG_ENABLE_VBLANK)) { return ~(0UL); }

  // Calculate the number of cycles until the next NMI.
  // NMI's can occur on cycle 1 of scanline 241.
  size_t cycles = 0;
  if ((current_scanline_ >= 241) && (current_cycle_ > 1)) {
    // We need to account for the scanline counter wrapping and the cycle
    // skipped at the end of even frames.
    cycles = ((nmi_line * cycles_per_line) + nmi_cycle)
           + ((lines_per_frame - current_scanline_ - 1) * cycles_per_line)
           + (cycles_per_line - current_cycle_)
           - (!frame_odd_);
  } else {
    cycles = ((nmi_line * cycles_per_line) + nmi_cycle)
           - ((current_scanline_ * cycles_per_line) + current_cycle_);
  }

  // Convert PPU cycles to CPU cycles, taking the floor of the result.
  return static_cast<size_t>((static_cast<float>(cycles) * 0.33f) - 0.33f);
}

/*
 * Runs the next cycle in the ppu emulation, then increments the cycle/scanline
 * counters.
 *
 * Assumes Connect() has already been called.
 */
void Ppu::RunSchedule(size_t cycles) {
  // Check if rendering has been disabled by the software.
  size_t delta = 0;
  if (IsDisabled()) {
    while (cycles > 0) {
      delta = CalculateCounters(cycles);
      Disabled(delta);
      UpdateCounters();
    }
  } else {
    // Otherwise, render video normally.
    while (cycles > 0) {
      delta = CalculateCounters(cycles);
      Render(delta);
      UpdateCounters();
    }
  }

  // Pull the NMI line high if one should be generated.
  Signal();

  return;
}

/*
 * Returns true when rendering is disabled.
 */
bool Ppu::IsDisabled(void) {
  return !(mask_ & FLAG_RENDER_BG) && !(mask_ & FLAG_RENDER_SPRITES);
}

/*
 * Takes in a number of cycles which must be run, and uses them to return
 * how many cycles should be completed to either reach that number or finish
 * the current scanline. Updates the number of cycles which should be executed.
 * Calculates the values of the PPU's positional counters following the execution
 * of the given number of cycles.
 */
size_t Ppu::CalculateCounters(size_t &cycles) {
  // Determine if we are on a scanline which will terminate early.
  size_t cycle_max = 0;
  if (!frame_odd_ && (current_scanline_ >= 261) && !IsDisabled()) {
    cycle_max = 340;
  } else {
    // Otherwise, we perform the normal calculation.
    cycle_max = 341;
  }

  // Calculate how many cycles the user should run to finish this scanline.
  size_t exec = MIN(cycle_max - current_cycle_, cycles);

  // Update the scanline counter and user cycles remaining.
  bool finish_scanline = ((exec + current_cycle_) >= cycle_max);
  cycles -= exec;
  next_current_cycle_ = (finish_scanline) ? 0 : exec + current_cycle_;
  next_current_scanline_ = current_scanline_ + finish_scanline;
  next_frame_odd_ = frame_odd_;

  // Wrap the scanline and toggle the frame if it is time to do so.
  if (next_current_scanline_ > 261) {
    next_current_scanline_ = 0;
    next_frame_odd_ = !frame_odd_;
  }

  return exec;
}

/*
 * Applies the updates to the PPU's positional counters generated by
 * CalculateCounters().
 */
void Ppu::UpdateCounters(void) {
  current_cycle_ = next_current_cycle_;
  current_scanline_ = next_current_scanline_;
  frame_odd_ = next_frame_odd_;
  return;
}

/*
 * Performs any PPU actions which cannot be disabled.
 */
void Ppu::Disabled(size_t delta) {
  // Determine which action should be performed.
  if ((current_scanline_ >= 8) && (current_scanline_ < 232)
       && (current_cycle_ <= 256) && ((current_cycle_ + delta) > 1)) {
    // Draws the background color to the screen when rendering is disabled.
    DrawBackground(delta);
  } else if ((current_scanline_ >= 240) && (current_scanline_ < 261)) {
    // Signals the start of vblanks.
    RenderBlank(delta);
  } else if ((current_scanline_ >= 261) && (current_cycle_ <= 1)
         && ((current_cycle_ + delta) > 1)) {
    // Resets the status register during the pre-render scanline.
    status_ = 0;
  }

  return;
}

/*
 * Draws the background color to the screen when rendering is disabled.
 *
 * Assumes PPU rendering is disabled.
 */
void Ppu::DrawBackground(size_t delta) {
  // Get the universal background color address and the screen position.
  size_t start_cycle = MAX(1, current_cycle_);
  size_t screen_x = start_cycle - 1;
  size_t screen_y = current_scanline_;
  DoubleWord color_addr = 0;

  // Determine how many pixels should be rendered
  size_t num_pixels = MIN(257 - start_cycle, (current_cycle_ + delta)
                                              - start_cycle);

  // The universal background color can be changed using the current vram
  // address.
  if ((vram_addr_ & VRAM_BUS_MASK) > PALETTE_BASE_ADDR) {
    color_addr = vram_addr_ & PALETTE_ADDR_MASK;
  }

  // Render the background.
  Pixel bg_color = pixel_data_[color_addr];
  Pixel pixel_buf[kScreenWidth_];
  for (size_t i = 0; i < num_pixels; i++) { pixel_buf[i] = bg_color; }
  renderer_->DrawPixels(screen_y, screen_x, pixel_buf, num_pixels);

  return;
}


/*
 * Runs the rendering action for the given cycle/scanline.
 */
void Ppu::Render(size_t delta) {
  // Determine which scanline we're on and render accordingly.
  if (current_scanline_ < 240) {
    RenderVisible(delta);
  } else if (current_scanline_ < 261) {
    RenderBlank(delta);
  } else {
    RenderPre(delta);
  }

  return;
}

/*
 * Performs the requested number of cycles for a visible scanline.
 */
void Ppu::RenderVisible(size_t delta) {
  // Finish the garbage nametable write that got skipped.
  if ((current_cycle_ == 0) && delta) { mdr_write_ = false; }

  // Check if this delta should clear the SOAM buffer.
  if ((current_cycle_ <= 1) && ((current_cycle_ + delta) > 1)) {
    EvalClearSoam();
  }

  // Update sprite evaluation for this scanline.
  // FIXME: Should run over time from (64, 256].
  if ((current_cycle_ <= 65) && ((current_cycle_ + delta) > 65) && delta) {
    EvalSprites();
  }

  // Draw the pixels for this scanline.
  if ((current_cycle_ <= 256) && ((current_cycle_ + delta) > 1) && delta) {
    RenderUpdateFrame(delta, true);
  }

  // Updates the vram address and sprite buffers at the correct time.
  if ((current_cycle_ <= 320) && ((current_cycle_ + delta) > 257) && delta) {
    // On cycle 257, the horizontal vram position is loaded from the temp vram
    // address register. As an optmization, sprite fetching is run only on this
    // cycle.
    if (current_cycle_ <= 257) {
      RenderUpdateHori();
      soam_render_buf_ = !soam_render_buf_;
    }
    // The OAM addr is reset to zero during sprite prep, which has been
    // optimized out.
    oam_addr_ = 0;
  }

  // Fetch the background tile data for the next scanline.
  if ((current_cycle_ <= 336) && ((current_cycle_ + delta) > 321) && delta) {
    RenderFetchTiles(delta, true);
  }

  // Perform the unused nametable byte fetches. Some mappers may clock this.
  if ((current_cycle_ <= 340) && ((current_cycle_ + delta) > 337) && delta) {
    // Unused NT byte fetches, mappers may clock this.
    RenderDummyNametableAccess(delta);
  }

  return;
}

/*
 * Executes the specified number of cycles in the drawing phase of the current
 * scanline. No more than 257 - current_cycle_ cycles will be executed.
 * The execution of these cycles includes filling the tile buffer, drawing
 * pixels to the screen, and possibly incrementing the Y-coord of VRAM.
 */
void Ppu::RenderUpdateFrame(size_t delta, bool output) {
  // Update the tile buffer.
  RenderFetchTiles(delta, false);

  // Render the pixels to the screen.
  if (output) { RenderDrawPixels(delta); }

  // On cycle 256, the vertical vram position is incremented.
  if ((current_cycle_ <= 256) && ((current_cycle_ + delta) > 256)) {
    RenderYinc();
  }

  return;
}

/*
 * Fills the tile buffer for the given number of cycles.
 * If alt_buffer is true, the tiles are placed at the beginning of the
 * buffer for use with the next scanline.
 *
 * Assumes the current cycle is either between (320, 336] or [1, 256].
 */
void Ppu::RenderFetchTiles(size_t delta, bool alt_buffer) {
  // Determine if we are fetching tiles for the current or next scanline.
  size_t phase_delta, buffer_pos, start_cycle;
  if (alt_buffer) {
    // Fetching for the next scanline.
    start_cycle = MAX(321, current_cycle_);
    phase_delta = MIN(337 - start_cycle, (current_cycle_ + delta)
                                          - start_cycle);
    buffer_pos = start_cycle - 320;
  } else if (current_cycle_ <= 255) {
    // Fetching for this scanline.
    // Cycle 256 is skipped, as it is unused.
    start_cycle = MAX(1, current_cycle_);
    phase_delta = MIN(256 - start_cycle, (current_cycle_ + delta)
                                          - start_cycle);
    // During the drawing phase, the buffer is filled two tiles ahead.
    buffer_pos = start_cycle + 16;
  } else {
    // Out of range for tile fetching.
    return;
  }

  // Execute the requested cycles.
  for (size_t i = buffer_pos; i < buffer_pos + phase_delta; i++) {
    // The PPU loads in a tile over the course of 8 cycles.
    switch (i & REG_UPDATE_MASK) {
      case REG_APPLY_UPDATES:
        // We need to align the address before we can use it to update
        // the tile buffer.
        RenderUpdateTileBuffer(i - 8);
        RenderXinc();
        break;
      case REG_FETCH_NT:
        mdr_ = memory_->VramRead((vram_addr_ & VRAM_NT_ADDR_MASK)
                                             | PPU_NT_OFFSET);
        break;
      case REG_FETCH_AT:
        next_palette_ = RenderGetAttribute();
        break;
      case REG_FETCH_TILE_LOW:
        next_tile_[0] = RenderGetTile(mdr_, false);
        break;
      case REG_FETCH_TILE_HIGH:
        next_tile_[1] = RenderGetTile(mdr_, true);
        break;
      default:
        break;
    }
  }

  return;
}

/*
 * Updates the tile buffer with the next 8 pixels of palette and tile data.
 */
void Ppu::RenderUpdateTileBuffer(size_t buffer_pos) {
  // Weaves the two planes of tile data into one variable.
  DataWord odd_tile_data = ((next_tile_[0] & 0xAA) >> 1)
                         | (next_tile_[1] & 0xAA);
  DataWord even_tile_data = (next_tile_[0] & 0x55)
                          | ((next_tile_[1] & 0x55) << 1);

  // Prepare the palette data to be buffered.
  DataWord palette_latch = next_palette_ << 2;

  // Add the tile data to the buffer. Each tile index is two bits.
  // If the tile pattern is zero, the palette of that pixel is zero'd out.
  DataWord pattern;
  for (int i = 0; i < 4; i++) {
    CONTRACT(buffer_pos <= kTileBufferSize_);
    // Add the odd tile to the buffer.
    pattern = ((odd_tile_data >> 6) & 0x3U);
    pattern = (pattern) ? pattern | palette_latch : 0U;
    tile_buffer_[buffer_pos++] = pattern;
    odd_tile_data <<= 2;

    CONTRACT(buffer_pos <= kTileBufferSize_);
    // Add the even tile to the buffer.
    pattern = ((even_tile_data >> 6) & 0x3U);
    pattern = (pattern) ? pattern | palette_latch : 0U;
    tile_buffer_[buffer_pos++] = pattern;
    even_tile_data <<= 2;
  }

  return;
}

/*
 * Uses the current vram address and cycle/scanline position to load
 * attribute palette bits from the nametable.
 */
DataWord Ppu::RenderGetAttribute(void) {
  // Get the coarse x and y from vram.
  DoubleWord coarse_x = vram_addr_ & COARSE_X_MASK;
  DoubleWord coarse_y = (vram_addr_ & COARSE_Y_MASK) >> 5;

  // Use the screen position to calculate the attribute table offset.
  DoubleWord attribute_offset = (coarse_x >> 2) | ((coarse_y >> 2) << 3);

  // Use the offset to calculate the address of the attribute table byte.
  DoubleWord attribute_addr = ATTRIBUTE_BASE_ADDR | attribute_offset
                            | (vram_addr_ & SCROLL_NT_MASK);
  DataWord attribute = memory_->VramRead(attribute_addr);

  // Isolate the color bits for the current quadrent the screen is drawing to.
  DataWord attribute_x_shift = coarse_x & 2U;
  DataWord attribute_y_shift = (coarse_y & 2U) << 1;
  return (attribute >> (attribute_x_shift | attribute_y_shift)) & 3U;
}

/*
 * Uses the given index, and plane high toggle, to calculate the pattern table
 * address of a tile from its nametable byte. Returns the byte at said pattern
 * table address.
 */
DataWord Ppu::RenderGetTile(DataWord index, bool plane_high) {
  // Get the value of fine Y, to be used as a tile offset.
  DoubleWord tile_offset = (vram_addr_ & FINE_Y_MASK) >> FINE_Y_SHIFT;

  // Use the plane toggle to get the plane bit in the vram address.
  DoubleWord tile_plane = (plane_high) ? 0x08U : 0x00U;

  // Get the index in the vram address from the provided index.
  DoubleWord tile_index = (static_cast<DoubleWord>(index)) << 4U;

  // Get the side of the pattern table to be used from the control register.
  DoubleWord tile_table = (ctrl_ & FLAG_BG_TABLE) ? PATTERN_TABLE_HIGH
                                                  : PATTERN_TABLE_LOW;

  // Calculate the vram address of the tile byte and return the tile byte.
  DoubleWord tile_address = tile_table | tile_index | tile_plane | tile_offset;
  return memory_->VramRead(tile_address);
}

/*
 * Uses the shift registers and sprite memory to draw the current cycles
 * pixel to the screen
 *
 * Assumes the PPU is currently running a cycle between 1 and 256 (inclusive)
 * of a visible scanline.
 */
void Ppu::RenderDrawPixels(size_t delta) {
  // Get the rendering information.
  size_t start_cycle = MAX(1, current_cycle_);
  size_t screen_x = start_cycle - 1;
  size_t screen_y = current_scanline_;
  size_t num_pixels = MIN(257 - start_cycle, (current_cycle_ + delta)
                                              - start_cycle);
  CONTRACT(start_cycle + num_pixels <= 257);
  DataWord *soam_buf = &(soam_buffer_[soam_render_buf_][screen_x]);
  DataWord *tile_buf = &(tile_buffer_[screen_x + fine_x_]);
  DataWord tile, sprite;

  // If we're on a scanline hidden by overscan, we need only check for
  // a sprite-zero hit.
  if ((current_scanline_ < 8) || (current_scanline_ >= 232)) {
    if ((mask_ & FLAG_RENDER_BG) && (mask_ & FLAG_RENDER_SPRITES)
                                 && !(status_ & FLAG_HIT)) {
      for (size_t i = 0; i < num_pixels; i++) {
        tile = ((mask_ & FLAG_LEFT_BG) || (i + screen_x >= 8))
             ? tile_buf[i] : 0U;
        sprite = ((mask_ & FLAG_LEFT_SPRITES) || ((i + screen_x) >= 8))
               ? soam_buf[i] : 0U;
        if ((sprite & FLAG_SOAM_BUFFER_ZERO) && tile
                    && ((i + screen_x) != 255)) {
          status_ |= FLAG_HIT;
          break;
        }
      }
    }
    return;
  }

  // Used to buffer the pixels to be rendered.
  Pixel pixel_buf[kScreenWidth_];

  // Determine which pieces of rendering are enabled. This function is not
  // called when rendering is disabled, so we need not check for that.
  DataWord *line_buf;
  if ((mask_ & FLAG_RENDER_BG) && (mask_ & FLAG_RENDER_SPRITES)) {
    // All rendering is enabled, so we must scan tiles into the buffer.
    line_buf = soam_buf;
    for (size_t i = 0; i < num_pixels; i++) {
      tile = ((mask_ & FLAG_LEFT_BG) || (i + screen_x >= 8))
           ? tile_buf[i] : 0U;
      sprite = ((mask_ & FLAG_LEFT_SPRITES) || ((i + screen_x) >= 8))
             ? soam_buf[i] : 0U;
      // Check if this pixel has a sprite that should be drawn.
      line_buf[i] = ((sprite & FLAG_SOAM_BUFFER_PRIORITY) || !tile)
                  ? (sprite & FLAG_SOAM_BUFFER_PALETTE) : tile;

      // Check if this is a sprite zero hit.
      if ((sprite & FLAG_SOAM_BUFFER_ZERO) && tile
                  && ((i + screen_x) != 255)) {
        status_ |= FLAG_HIT;
      }
    }

    // Fill the pixel buffer with the pixels corresponding to the line buffer.
    for (size_t i = 0; i < num_pixels; i++) {
      pixel_buf[i] = pixel_data_[line_buf[i]];
    }
  } else if (mask_ & FLAG_RENDER_BG) {
    // Sprites are disabled, so we just draw the contents of the tile buffer.
    line_buf = tile_buf;

    // Fill the pixel buffer with the data from the line buffer.
    for (size_t i = 0; i < num_pixels; i++) {
      // We need to check if left background have been disabled.
      // We do this check instead of overwriting the tile buffer and having
      // one section of code to get the pixel buffer because the tile buffer
      // must not be changed by this function.
      pixel_buf[i] = (((screen_x + i) >= 8) || (mask_ & FLAG_LEFT_BG))
                   ? pixel_data_[line_buf[i]] : pixel_data_[0];
    }
  } else {
    // Backgrounds are disabled, so we just draw the sprite buffer.
    line_buf = soam_buf;

    // Fill the pixel buffer with the data from the line buffer.
    // We may need to disable left sprite rendering here.
    for (size_t i = 0; i < num_pixels; i++) {
      pixel_buf[i] = (((screen_x + i) >= 8) || (mask_ & FLAG_LEFT_SPRITES))
                   ? pixel_data_[line_buf[i]] : pixel_data_[0];
    }
  }

  // Render the pixels to the screen.
  renderer_->DrawPixels(screen_y, screen_x, pixel_buf, num_pixels);

  return;
}

/*
 * Updates the horizontal piece of the vram address.
 */
void Ppu::RenderUpdateHori(void) {
  // Copies the coarse X and horizontal nametable bit from t to v.
  vram_addr_ = (vram_addr_ & (SCROLL_Y_MASK | SCROLL_VNT_MASK))
             | (temp_vram_addr_ & (SCROLL_X_MASK | SCROLL_HNT_MASK));
  return;
}

/*
 * Executes 2 dummy nametable fetches over 4 cycles.
 */
void Ppu::RenderDummyNametableAccess(size_t delta) {
  size_t start_cycle = MAX(337, current_cycle_);
  size_t num_cycles = MIN(341 - start_cycle, (current_cycle_ + delta)
                                              - start_cycle);
  // Determine which cycle of the fetch we are on and execute it.
  for (size_t i = 0; i < num_cycles; i++) {
    if (!mdr_write_) {
      mdr_ = memory_->VramRead((vram_addr_ & VRAM_NT_ADDR_MASK)
                                           | PPU_NT_OFFSET);
    }
    mdr_write_ = !mdr_write_;
  }

  return;
}

/*
 * Performs a coarse X increment on the vram address. Accounts for the
 * nametable bits.
 */
void Ppu::RenderXinc(void) {
  // Increment the coarse X.
  DoubleWord xinc = (vram_addr_ & COARSE_X_MASK) + 1;

  // When coarse X overflows, bit 10 (horizontal nametable select) is flipped.
  vram_addr_ = ((vram_addr_ & ~COARSE_X_MASK)
             | (xinc & COARSE_X_MASK))
             ^ ((xinc & COARSE_X_CARRY_MASK) << TOGGLE_HNT_SHIFT);
  return;
}

/*
 * Performs a Y increment on the vram address. Accounts for both coarse Y,
 * fine Y, and the nametable bits.
 */
void Ppu::RenderYinc(void) {
  // Increment fine Y.
  vram_addr_ = (vram_addr_ & VRAM_ADDR_MASK) + FINE_Y_INC;

  // Add overflow to coarse Y.
  vram_addr_ = (vram_addr_ & ~COARSE_Y_MASK)
             | ((vram_addr_ + ((vram_addr_ & FINE_Y_CARRY_MASK)
             >> FINE_Y_CARRY_SHIFT)) & COARSE_Y_MASK);

  // The vertical name table bit should be toggled if coarse Y was incremented
  // to 30.
  if ((vram_addr_ & SCROLL_Y_MASK) == Y_INC_OVERFLOW) {
    vram_addr_ ^= SCROLL_VNT_MASK;
    vram_addr_ &= ~SCROLL_Y_MASK;
  }

  return;
}

/*
 * Performs the rendering action during vertical blank, which consists only
 * of signaling an NMI on (1,241).
 */
void Ppu::RenderBlank(size_t delta) {
  if ((current_scanline_ == 241) && (current_cycle_ <= 1)
                                 && ((current_cycle_ + delta) > 1)) {
    // TODO: Implement special case timing.
    status_ |= FLAG_VBLANK;
    renderer_->DrawFrame();
  }
  return;
}

/*
 * Execute the pre-render scanline acording to the current cycle.
 * Resets to the status flags are performed here.
 */
void Ppu::RenderPre(size_t delta) {
  // The status flags are reset at the begining of the pre-render scanline.
  if ((current_cycle_ <= 1) && ((current_cycle_ + delta) > 1)) {
    status_ = 0;
  }

  // Rendering accesses are made, but nothing is drawn.
  if ((current_cycle_ <= 256) && ((current_cycle_ + delta) > 1) && delta) {
    RenderUpdateFrame(delta, false);
  }

  // The horizontal address is updated on this cycle.
  if ((current_cycle_ <= 257) && ((current_cycle_ + delta) > 257)) {
    RenderUpdateHori();
  }

  // The vertical address is updated throughout these cycles.
  if ((current_cycle_ <= 304) && ((current_cycle_ + delta) > 280) && delta) {
    RenderUpdateVert();
  }

  // Fetches the background tiles for the next scanline.
  if ((current_cycle_ <= 336) && ((current_cycle_ + delta) > 321) && delta) {
    RenderFetchTiles(delta, true);
  }

  // Unused NT byte fetches, which may be clocked by mappers.
  if ((current_cycle_ <= 340) && ((current_cycle_ + delta) > 337) && delta) {
    RenderDummyNametableAccess(delta);
  }

  // The OAM addr is reset here to prepare for sprite evaluation on the next
  // scanline.
  if (current_cycle_ > 256 && current_cycle_ <= 320) { oam_addr_ = 0; }

  return;
}

/*
 * Updates the vertical piece of the vram address.
 */
void Ppu::RenderUpdateVert(void) {
  // Copies the fine Y, coarse Y, and vertical nametable bit from t to v.
  vram_addr_ = (vram_addr_ & (SCROLL_X_MASK | SCROLL_HNT_MASK))
             | (temp_vram_addr_ & (SCROLL_Y_MASK | SCROLL_VNT_MASK));
  return;
}

/*
 * Sets every value in secondary OAM to 0. Used in sprite evaluation.
 */
void Ppu::EvalClearSoam(void) {
  for (size_t i = 0; i < SOAM_BUFFER_SIZE; i++) {
    soam_buffer_[!soam_render_buf_][i] = 0;
  }
  return;
}

/*
 * Performs sprite evaluation for the given cycle, implementing the
 * incorrect sprite overflow behavior (partially). This function should
 * only be called from ppu_eval().
 *
 * FIXME: This function should run over time, not all at once.
 */
void Ppu::EvalSprites(void) {
  // Run sprite evaluation. This loop fills the SOAM scanline buffer,
  // which contains one byte of sprite data for each pixel to be rendered.
  size_t i = oam_addr_;
  size_t sprites_found = 0;
  while (i < PRIMARY_OAM_SIZE) {
    // Read in the sprite and check if it was in range.
    if (sprites_found >= 8) {
      // More than 8 sprites have been found, so spirte overflow begins.
      if (EvalInRange(primary_oam_[i])) {
        // Flag that overflow occured.
        // FIXME: Should not be set immediately.
        status_ |= FLAG_OVERFLOW;
        break;
      } else {
        // Sprite overflow is bugged, and does not increment correctly.
        i += 5;
      }
    } else if (EvalInRange(primary_oam_[i])) {
      // If it was, read it into the scanline buffer.
      sprites_found++;
      EvalFillSoamBuffer(&(primary_oam_[i]), i == oam_addr_);
      i += 4;
    } else {
      // Otherwise, the sprite was out of range, so we move on.
      i += 4;
    }
  }
  oam_addr_ = i;

  return;
}

/*
 * Checks if a sprite is visible on the current scanline using the value
 * stored in the eval buffer as a Y cordinate.
 */
bool Ppu::EvalInRange(DataWord sprite_y) {
  // Get the current size of sprites (8x8 or 8x16) from the control register
  // and the screen y coordinate from the current scanline.
  DataWord sprite_size = (ctrl_ & FLAG_SPRITE_SIZE) ? 16 : 8;
  DataWord screen_y = current_scanline_;

  // Check if the sprite is visible on this scanline.
  bool in_range = (sprite_y <= screen_y) && (sprite_y < 240)
                      && (screen_y < (sprite_y + sprite_size));
  return in_range;
}

/*
 * Adds the given sprite to the soam sprite buffer, setting its sprite zero
 * bit if is_zero is set.
 */
void Ppu::EvalFillSoamBuffer(DataWord *sprite_data, bool is_zero) {
  // Setup the information that will be contained in each scanline buffer byte
  // for this sprite. This byte contains whether or not the pixel belongs to
  // sprite zero, the sprites priority, and its palette index.
  DataWord base_byte = SPRITE_PALETTE_BASE;
  if (is_zero) { base_byte |= FLAG_SOAM_BUFFER_ZERO; }
  if (!(sprite_data[2] & FLAG_SPRITE_PRIORITY)) {
    base_byte |= FLAG_SOAM_BUFFER_PRIORITY;
  }
  base_byte |= (sprite_data[2] & FLAG_SPRITE_PALETTE) << 2;

  // Get the individual pixels for the sprite.
  DataWord pat_lo, pat_hi;
  EvalGetSprite(sprite_data, &pat_lo, &pat_hi);

  // Add the sprite to the soam buffer.
  DataWord sprite_x = sprite_data[3];
  for (size_t i = sprite_x; (i < sprite_x + 8U)
                         && (i < SOAM_BUFFER_SIZE); i++) {
    // The sprite pixel should only be added to the buffer if no other sprite
    // has been rendered to that pixel.
    DataWord buf_byte = soam_buffer_[!soam_render_buf_][i];
    if (!buf_byte)  {
      DataWord sprite_pat = ((pat_hi >> 6U) & 2U) | ((pat_lo >> 7U) & 1U);
      soam_buffer_[!soam_render_buf_][i] = (sprite_pat)
                                         ? base_byte | sprite_pat : 0U;
    }

    // Shift the data to the next sprite pixel.
    pat_lo <<= 1U;
    pat_hi <<= 1U;
  }

  return;
}

/*
 * Fetches the pattern bytes for a given sprite.
 */
void Ppu::EvalGetSprite(DataWord *sprite_data, DataWord *pat_lo,
                                               DataWord *pat_hi) {
  // Get some basic information about the current sprite being prepared.
  DataWord screen_y = current_scanline_;
  DataWord sprite_y = sprite_data[0];

  // Calculate position and offset from the sprite.
  DoubleWord tile_index = sprite_data[1];
  // If the sprite is 8x16, and the bottom half is being rendered,
  // we need to move to the next tile. An offset is calculated to do this.
  DoubleWord index_offset = 0;
  if ((ctrl_ & FLAG_SPRITE_SIZE) && (screen_y >= (sprite_y + 8U))) {
    index_offset = X16_INDEX_OFFSET;
    sprite_y += 8;
  }
  // This tile offset determines which of the 8 rows of the tile will be
  // returned.
  DoubleWord tile_offset = screen_y - sprite_y;
  CONTRACT(tile_offset < 8);

  // Check if the sprite is being flipped vertically.
  if (sprite_data[2] & FLAG_SPRITE_VFLIP) {
    tile_offset = (~tile_offset) & 0x07U;
    if (ctrl_ & FLAG_SPRITE_SIZE) { index_offset ^= X16_INDEX_OFFSET; }
  }

  // Determine which size of sprites are being used and then
  // calculate the pattern address.
  DoubleWord tile_addr;
  if (ctrl_ & FLAG_SPRITE_SIZE) {
    tile_addr = tile_offset
              | ((tile_index & X16_TILE_MASK) << X16_TILE_SHIFT)
              | ((tile_index & X16_TABLE_MASK) << X16_TABLE_SHIFT)
              | index_offset;
  } else {
    DoubleWord tile_table = (ctrl_ & FLAG_SPRITE_TABLE) ? PATTERN_TABLE_HIGH
                                                        : PATTERN_TABLE_LOW;
    tile_addr = tile_offset | (tile_index << X8_TILE_SHIFT) | tile_table;
  }

  // Use the calculated pattern address to get the tile bytes.
  *pat_lo = memory_->VramRead(tile_addr);
  *pat_hi = memory_->VramRead(tile_addr | SPRITE_PLANE_HIGH_MASK);

  // Check if the bytes should be horizontally flipped.
  if (sprite_data[2] & FLAG_SPRITE_HFLIP) {
    *pat_lo = ReverseWord(*pat_lo);
    *pat_hi = ReverseWord(*pat_hi);
  }

  return;
}

/*
 * Generates an NMI in the CPU emulation when appropriate.
 */
void Ppu::Signal(void) {
  // NMIs should be generated when they are enabled in ppuctrl and
  // the ppu is in vblank.
  *nmi_line_ = (ctrl_ & FLAG_ENABLE_VBLANK) && (status_ & FLAG_VBLANK);
  return;
}

/*
 * Takes in an address from cpu memory and uses it to write to the
 * corresponding mmio in the ppu.
 */
void Ppu::Write(DoubleWord reg_addr, DataWord val) {
  // Fill the PPU bus with the value being written.
  bus_ = val;

  // Determine which register is being accessed.
  switch(reg_addr & PPU_MMIO_MASK) {
    case PPU_CTRL_ACCESS:
      ctrl_ = val;
      // Update the scrolling nametable selection.
      temp_vram_addr_ = (temp_vram_addr_ & ~SCROLL_NT_MASK)
                      | ((static_cast<DoubleWord>(ctrl_ & FLAG_NAMETABLE))
                      << SCROLL_NT_SHIFT);
      break;
    case PPU_MASK_ACCESS:
      MmioMaskWrite(val);
      break;
    case PPU_STATUS_ACCESS:
      // Read only.
      break;
    case OAM_ADDR_ACCESS:
      // TODO: OAM Corruption on write.
      oam_addr_ = val;
      break;
    case OAM_DATA_ACCESS:
      OamDma(val);
      break;
    case PPU_SCROLL_ACCESS:
      MmioScrollWrite(val);
      break;
    case PPU_ADDR_ACCESS:
      MmioAddrWrite(val);
      break;
    case PPU_DATA_ACCESS:
      memory_->VramWrite(vram_addr_, val);
      MmioVramAddrInc();
      break;
  }

  return;
}

/*
 * Updates the mask register and palette. Clears the sprite/tile buffers if
 * rendering for either has been disabled. Doing this makes the assumption
 * that the shift registers continued to empty when rendering was disabled
 * in original hardware. TODO: Confirm this is the case.
 */
void Ppu::MmioMaskWrite(DataWord val) {
  // Update the mask value and palette.
  mask_ = val;
  memory_->PaletteUpdate(val);

  // Clear the tile and sprite buffers if either has now been disabled.
  if (!(mask_ & FLAG_RENDER_BG)) {
    for (size_t i = 0; i < kTileBufferSize_; i++) { tile_buffer_[i] = 0U; }
  }
  if (!(mask_ & FLAG_RENDER_SPRITES)) {
    for (size_t i = 0; i < 2; i++) {
      for (size_t j = 0; j < SOAM_BUFFER_SIZE; j++) {
        soam_buffer_[i][j] = 0;
      }
    }
  }

  return;
}

/*
 * Writes the the PPU scroll register. Toggles the write bit.
 */
void Ppu::MmioScrollWrite(DataWord val) {
  // Determine which write should be done.
  if (write_toggle_) {
    // Update scroll Y.
    temp_vram_addr_ = (temp_vram_addr_
        & (SCROLL_X_MASK | SCROLL_NT_MASK))
        | (((static_cast<DoubleWord>(val)) << COARSE_Y_SHIFT) & COARSE_Y_MASK)
        | (((static_cast<DoubleWord>(val)) << FINE_Y_SHIFT) & FINE_Y_MASK);
    write_toggle_ = false;
  } else {
    // Update scroll X.
    fine_x_ = val & FINE_X_MASK;
    temp_vram_addr_ = (temp_vram_addr_ & (SCROLL_Y_MASK | SCROLL_NT_MASK))
                    | (val >> COARSE_X_SHIFT);
    write_toggle_ = true;
  }

  return;
}

/*
 * Writes to the PPU addr register. Toggles the write bit.
 */
void Ppu::MmioAddrWrite(DataWord val) {
  // Determine which write should be done.
  if (write_toggle_) {
    // Write the low byte and update v.
    temp_vram_addr_ = (temp_vram_addr_ & PPU_ADDR_HIGH_MASK) | val;
    vram_addr_ = temp_vram_addr_;
    write_toggle_ = false;
  } else {
    // Write the high byte.
    temp_vram_addr_ = (temp_vram_addr_ & PPU_ADDR_LOW_MASK)
        | (((static_cast<DoubleWord>(val)) << PPU_ADDR_HIGH_SHIFT)
        & PPU_ADDR_HIGH_MASK);
    write_toggle_ = true;
  }

  return;
}

/*
 * Increments the vram address after a PPU data access, implementing the
 * buggy behavior expected during rendering.
 */
void Ppu::MmioVramAddrInc(void) {
  // Determine how the increment should work.
  if (IsDisabled() || (current_scanline_ >= 240
                   && current_scanline_ <= 260)) {
    // When the PPU is inactive, vram is incremented correctly.
    vram_addr_ = (ctrl_ & FLAG_VRAM_VINC) ? (vram_addr_ + 32)
                                          : (vram_addr_ + 1);
  } else {
    // Writing to PPU data during rendering causes a X and Y increment.
    // This only happens when the PPU would not otherwise be incrementing them.
    if (!((((current_cycle_ > 0) && (current_cycle_ <= 256))
         || (current_cycle_ > 320)) && ((current_cycle_ & 0x7) == 0))) {
      RenderXinc();
    }
    if (current_cycle_ != 256) {
      RenderYinc();
    }
  }

  return;
}

/*
 * Takes in an address from cpu memory and uses it to read from
 * the corresponding mmio in the ppu.
 */
DataWord Ppu::Read(DoubleWord reg_addr) {
  // Determine which register is being read from.
  switch(reg_addr & PPU_MMIO_MASK) {
    case PPU_STATUS_ACCESS:
      // PPU status contains only 3 bits (high 3).
      bus_ = (bus_ & ~PPU_STATUS_MASK)
           | (status_ & PPU_STATUS_MASK);
      // Reads to PPU status reset the write toggle and clear the vblank flag.
      status_ &= ~FLAG_VBLANK;
      write_toggle_ = false;
      break;
    case PPU_DATA_ACCESS:
      // Reading from mappable VRAM (not the palette) returns the value to an
      // internal bus.
      if (reg_addr < PPU_PALETTE_OFFSET) {
        bus_ = vram_buf_;
        vram_buf_ = memory_->VramRead(vram_addr_);
      } else {
        bus_ = memory_->VramRead(vram_addr_);
        vram_buf_ = memory_->VramRead((vram_addr_ & VRAM_NT_ADDR_MASK)
                                                  | PPU_NT_OFFSET);
      }
      MmioVramAddrInc();
      break;
    case PPU_CTRL_ACCESS:
    case PPU_MASK_ACCESS:
    case OAM_ADDR_ACCESS:
    case OAM_DATA_ACCESS:
    case PPU_SCROLL_ACCESS:
    case PPU_ADDR_ACCESS:
    default:
      // Reading from a write only register simply yields the value on the bus.
      break;
  }

  // The requested value should now be on the bus, so we return.
  return bus_;
}

/*
 * Directly writes the given value to OAM, incrementing the OAM address.
 */
void Ppu::OamDma(DataWord val) {
  // Writes during rendering are ignored.
  if (IsDisabled() || (current_scanline_ >= 240
                   && current_scanline_ <= 260)) {
    primary_oam_[oam_addr_] = val;
    oam_addr_++;
  }

  // The PPU bus is filled with the value, incase we are coming from a CPU DMA.
  bus_ = val;
  return;
}

/*
 * Deletes the given PPU class.
 */
Ppu::~Ppu() {
  // Free the ppu structures.
  delete[] primary_oam_;
  delete[] soam_buffer_[0];
  delete[] soam_buffer_[1];
  delete[] tile_buffer_;

  return;
}
