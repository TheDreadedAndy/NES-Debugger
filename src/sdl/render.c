/*
 * TODO
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../util/data.h"
#include "./window.h"
#include "./render.h"
#include "../util/contracts.h"
#include "../ppu/palette.h"

/*
 * The NES draws a 256x240 pictures, which is padded to 280x240. Most tvs
 * display this picture as 280x224. These constants are used to scale the
 * SDL rendering surface to the appropriate size on the SDL window, given
 * this information about the NES.
 */
#define NES_WIDTH_OFFSET 0
#define NES_WIDTH 256
#define NES_HEIGHT_OFFSET 8
#define NES_TRUE_HEIGHT 224
#define NES_TRUE_WIDTH_RATIO (256.0 / 280.0)
#define NES_WIDTH_PAD_OFFSET_RATIO (12.0 / 280.0)
#define NES_W_TO_H (256.0 / 224.0)
#define NES_TRUE_H_TO_W (224.0 / 280.0)

/*
 * Set whenever the ppu draws a frame (which happens at vblank).
 * Reset whenever render_has_drawn() is called.
 * Used to track the frame rate of the emulator and, thus, throttle it.
 */
static bool frame_output = false;

/*
 * Set whenever the size of the window changes, which means the window
 * surface must be obtained again.
 */
static bool window_surface_valid = false;

/* Helper functions */
void render_get_window_rect(SDL_Surface *window_surface, SDL_Rect *window_rect);

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, uint32_t pixel) {
  CONTRACT(row < (size_t) render->h);
  CONTRACT(col < (size_t) render->w);

  // Write the given pixel to the given location.
  uint32_t *pixels = (uint32_t*) render->pixels;
  pixels[row * render->w + col] = pixel;

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

  // Setup the window surface.
  static SDL_Surface *window_surface = NULL;
  static SDL_Rect render_rect, window_rect;
  if (window_surface == NULL) {
    render_rect.x = NES_WIDTH_OFFSET;
    render_rect.y = NES_HEIGHT_OFFSET;
    render_rect.w = NES_WIDTH;
    render_rect.h = NES_TRUE_HEIGHT;
  }


  // Get the window surface, and recalculate the rect, if the surface is invalid.
  if (!window_surface_valid) {
    window_surface = SDL_GetWindowSurface(window);
    render_get_window_rect(window_surface, &window_rect);
    SDL_FillRect(window_surface, NULL, 0);
    window_surface_valid = true;
  }

  // Copy the render surface to the window surface.
  SDL_BlitScaled(render, &render_rect, window_surface, &window_rect);

  // Update the window.
  SDL_UpdateWindowSurface(window);

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
void render_get_window_rect(SDL_Surface *window_surface,
                            SDL_Rect *window_rect) {
  // Determine which dimension the destination window should be padded in.
  if ((NES_TRUE_H_TO_W * window_surface->w) > window_surface->h) {
    // Fill in height, pad in width.
    window_rect->h = window_surface->h;
    window_rect->y = 0;
    window_rect->w = NES_W_TO_H * window_rect->h;
    window_rect->x = (window_surface->w / 2) - (window_rect->w / 2);
  } else {
    // Fill in width, pad in height.
    window_rect->w = NES_TRUE_WIDTH_RATIO * window_surface->w;
    window_rect->x = NES_WIDTH_PAD_OFFSET_RATIO * window_rect->w;
    window_rect->h = NES_TRUE_H_TO_W * window_surface->w;
    window_rect->y = (window_surface->h / 2) - (window_rect->h / 2);
  }
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
 * Sets the value of window_surface_valid to false.
 * Called from the SDL event manager to invalidate the window.
 */
void render_invalidate_window_surface(void) {
  window_surface_valid = false;
  return;
}
