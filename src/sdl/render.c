/*
 * TODO
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../util/data.h"
#include "../util/util.h"
#include "./window.h"
#include "./render.h"
#include "../util/contracts.h"
#include "../ppu/palette.h"
#include "../ppu/ppu.h"

/*
 * The NES draws a 256x240 pictures, which is padded to 280x240. Most tvs
 * display this picture as 280x224. These constants are used to scale the
 * SDL rendering surface to the appropriate size on the SDL window, given
 * this information about the NES.
 */
#define NES_WIDTH 256
#define NES_HEIGHT 240
#define NES_TRUE_HEIGHT 224
#define NES_TRUE_WIDTH 280
#define NES_TRUE_H_TO_W (224.0 / 280.0)

/*
 * Set whenever the ppu draws a frame (which happens at vblank).
 * Reset whenever render_has_drawn() is called.
 * Used to track the frame rate of the emulator and, thus, throttle it.
 */
static bool frame_output = false;

/*
 * Set by the event manager whenever the size of the window changes.
 */
static bool window_size_valid = false;

/* Helper functions */
void render_set_draw_color(uint32_t color);
void render_update_renderer_scale(void);

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, word_t pixel) {
  CONTRACT(row < (size_t) NES_HEIGHT);
  CONTRACT(col < (size_t) NES_WIDTH);

  // Ignore the first/last 8 scanlines.
  if ((row < 8) || (row >= (NES_HEIGHT - 8))) { return; }

  // Render the pixel to the window.
  render_set_draw_color(palette_decode(pixel));
  SDL_RenderDrawPoint(render, col + 12, row - 8);

  return;
}

/*
 * Sets the SDL rendering color to the given xRGB color.
 */
void render_set_draw_color(uint32_t color) {
  word_t red, green, blue;
  red = (color >> 16) & WORD_MASK;
  green = (color >> 8) & WORD_MASK;
  blue = color & WORD_MASK;
  SDL_SetRenderDrawColor(render, red, green, blue, 0);
  return;
}

/*
 * Copies the rendering surface to the window.
 *
 * Assumes the window and render surface have been initialized.
 */
void render_frame(void) {
  CONTRACT(window != NULL);
  CONTRACT(render != NULL);

  // Recalculate the window rect if the window has been resized.
  if (!window_size_valid) {
    render_update_renderer_scale();
    window_size_valid = true;
  }

  // Update the window.
  SDL_RenderPresent(render);

  // Clear the window so the next frame can be drawn.
  render_set_draw_color(fill_color);
  SDL_RenderClear(render);

  // Signal that a frame was drawn.
  frame_output = true;

  return;
}

/*
 * Determines what the size of the window rect should be in order to
 * properly scale the NES picture to the window.
 *
 * Assumes the given surface and rect are non-null.
 */
void render_update_renderer_scale(void) {
  // Get the size of the window, to be used in the following calculation.
  int w, h;
  SDL_GetRendererOutputSize(render, &w, &h);

  // Determine how the renderer should be scaled.
  float scale;
  if ((NES_TRUE_H_TO_W * w) > h) {
    // Fill in height, pad in width.
    scale = ((float) h) / ((float) NES_TRUE_HEIGHT);
  } else {
    // Fill in width, pad in height.
    scale = ((float) w) / ((float) NES_TRUE_WIDTH);
  }

  // Update the renderer scaling.
  SDL_RenderSetScale(render, scale, scale);
}

/*
 * Returns the current value of frame_output, sets frame_output to false.
 * Used to track frame timing in the emulation.
 */
bool render_has_drawn(void) {
  bool frame = frame_output;
  frame_output = false;
  return frame;
}

/*
 * Sets the value of window_size_valid to false.
 * Called from the SDL event manager to invalidate the window.
 */
void render_invalidate_window_surface(void) {
  window_size_valid = false;
  return;
}
