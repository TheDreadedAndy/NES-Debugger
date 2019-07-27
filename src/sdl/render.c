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
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, word_t pixel) {
  CONTRACT(row < render->h);
  CONTRACT(col < render->w);

  // Lock the surface so we can safely edit pixel data.
  SDL_LockSurface(render);

  // Write the given pixel to the given location.
  uint32_t *pixels = (uint32_t*) render->pixels;
  pixels[row * render->w + col] = palette_decode(pixel);

  // Unlock the surface.
  SDL_UnlockSurface(render);

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
  SDL_BlitSurface(render, NULL, window_surface, NULL);

  // Update the window.
  SDL_UpdateWindowSurface(window);
  return;
}
