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
 * Set whenever the ppu draws a frame (which happens at vblank).
 * Reset whenever render_has_drawn() is called.
 * Used to track the frame rate of the emulator and, thus, throttle it.
 */
bool frame_output = false;

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, word_t pixel) {
  CONTRACT(row < (size_t) render->h);
  CONTRACT(col < (size_t) render->w);

  // Write the given pixel to the given location.
  uint32_t *pixels = (uint32_t*) render->pixels;
  pixels[row * render->w + col] = palette_decode(pixel);

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

  // Get the window surface.
  SDL_Surface *window_surface = SDL_GetWindowSurface(window);

  // Copy the render surface to the window surface.
  SDL_BlitScaled(render, NULL, window_surface, NULL);

  // Update the window.
  SDL_UpdateWindowSurface(window);

  // Signal that a frame was drawn.
  frame_output = true;

  return;
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
