/*
 * TODO
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "../util/contracts.h"
#include "../ppu/palette.h"

// Window size constants
#define NES_WIDTH 256
#define NES_HEIGHT 240

// Global sdl window variable, used to render to the game window, play sounds,
// and collect input. Cannot be directly accessed outside this file.
SDL_Window *window = NULL;

// Global SDL surface that pixels are rendered to before being drawn in the
// window. Cannot be directly accessed outside this file.
SDL_Surface *render = NULL;

/*
 * Sets up the SDL window for rendering and gathering input.
 */
bool window_init(void) {
  // Init SDL's video system.
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Failed to initialize SDL.\n");
    return false;
  }

  // Create window.
  window = SDL_CreateWindow("NES Debugger", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, NES_WIDTH, NES_HEIGHT, 0);

  // Check if the window was created successfully.
  if (window == NULL) {
    fprintf(stderr, "Failed to create SDL window.\n");
    return false;
  }

  // Create the rendering surface.
  render = SDL_CreateRGBSurface(0, NES_WIDTH, NES_HEIGHT, PALETTE_DEPTH,
                                PALETTE_RMASK, PALETTE_GMASK, PALETTE_BMASK, 0);

  // Verify that the surface was created successfully.
  if (render == NULL) {
    fprintf(stderr, "Failed to create SDL rendering surface.\n");
    return false;
  }

  // Return success.
  return true;
}

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void window_draw_pixel(size_t row, size_t col, word_t pixel) {
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
void window_draw_frame(void) {
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

/*
 * Closes the SDL window.
 *
 * Assumes the window and render surface have been initialized.
 */
void window_close(void) {
  CONTRACT(window != NULL);
  CONTRACT(render != NULL);

  SDL_DestroyWindow(window);
  SDL_FreeSurface(render);
  SDL_Quit();

  return;
}
