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
#define NES_WIDTH_OFFSET 0
#define NES_WIDTH 256
#define NES_HEIGHT 240
#define NES_HEIGHT_OFFSET 8
#define NES_TRUE_HEIGHT 224
#define NES_TRUE_WIDTH_RATIO (256.0 / 280.0)
#define NES_WIDTH_PAD_OFFSET_RATIO (12.0 / 280.0)
#define NES_W_TO_H (256.0 / 224.0)
#define NES_TRUE_H_TO_W (224.0 / 280.0)

/*
 * Holds the sizing values and offset of the main window.
 */
typedef struct window_size {
  // Scaled NES output size.
  int h;
  int w;
  // NES output offset.
  int x;
  int y;
  // Scaled NES pixel size.
  float dx;
  float dy;
} window_size_t;

/*
 * Holds the current window size and offsets.
 */
window_size_t *window_size = NULL;

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
static bool buffered_frame_valid = false;

/* Helper functions */
void render_set_draw_color(uint32_t color);
void render_update_window_size(void);

/*
 * Allocates the window size structure so that rendering may safely begin.
 */
void render_init(void) {
  window_size = xcalloc(1, sizeof(window_size_t));
  return;
}

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, word_t pixel) {
  CONTRACT(row < (size_t) NES_HEIGHT);
  CONTRACT(col < (size_t) NES_WIDTH);

  // Holds the current pixel size rect.
  static SDL_Rect pixel_rect;

  // If the first pixel is being drawn, validate the frame buffer.
  if ((row == 0) && (col == 0)) {
    buffered_frame_valid = true;
    pixel_rect.w = (((float) (size_t) window_size->dx) >= window_size->dx)
                 ? ((size_t) window_size->dx)
                 : ((size_t) window_size->dx) + 1;
    pixel_rect.h = (((float) (size_t) window_size->dy) >= window_size->dy)
                 ? ((size_t) window_size->dy)
                 : ((size_t) window_size->dy) + 1;
  } else if (!buffered_frame_valid) {
    // Otherwise, the window has been resized and a pixel should not be drawn.
    return;
  }

  // Determine the actual size of the pixel to be rendered.
  pixel_rect.x = window_size->x + ((size_t) (((float) col) * window_size->dx));
  pixel_rect.y = window_size->y + ((size_t) (((float) row) * window_size->dy));

  // Render the pixel to the window.
  render_set_draw_color(palette_decode(pixel));
  SDL_RenderFillRect(render, &pixel_rect);

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
    render_update_window_size();
    window_size_valid = true;
  }

  // Update the window if the buffered frame has not been invalidated
  // by a window resizing.
  if (buffered_frame_valid) { SDL_RenderPresent(render); }

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
void render_update_window_size(void) {
  // Get the size of the window, to be used in the following calculation.
  int w, h;
  SDL_GetWindowSize(window, &w, &h);

  // Determine which dimension the destination window should be padded in.
  if ((NES_TRUE_H_TO_W * w) > h) {
    // Fill in height, pad in width.
    window_size->h = h;
    window_size->y = 0;
    window_size->w = NES_W_TO_H * window_size->h;
    window_size->x = (w / 2) - (window_size->w / 2);
  } else {
    // Fill in width, pad in height.
    window_size->w = NES_TRUE_WIDTH_RATIO * w;
    window_size->x = NES_WIDTH_PAD_OFFSET_RATIO * window_size->w;
    window_size->h = NES_TRUE_H_TO_W * w;
    window_size->y = (h / 2) - (window_size->h / 2);
  }

  // Calculate the NES pixel size for the window.
  window_size->dx = ((float) window_size->w) / ((float) NES_WIDTH);
  window_size->dy = ((float) window_size->h) / ((float) NES_TRUE_HEIGHT);
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
  buffered_frame_valid = false;
  return;
}

/*
 * Frees the window size structure.
 */
void render_free(void) {
  free(window_size);
  return;
}
