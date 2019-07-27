/*
 * TODO
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "./window.h"
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
