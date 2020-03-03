/*
 * This is a software renderering implementation of the abstract renderer
 * class. It is implemented using and SDL surface to buffer the next frame
 * to. This surface is blitted to the screen whenever it is filled, and the
 * display is then updated.
 *
 * This implementation of rendering is faster on hardware with poor OpenGL
 * support.
 */

#include "./software_renderer.h"

#include <new>
#include <cstdio>
#include <cstdint>

#include <SDL2/SDL.h>

#include "../util/contracts.h"
#include "../memory/palette.h"
#include "./window.h"
#include "./renderer.h"

/*
 * Attempts to create a software rendering object.
 *
 * Returns NULL on failure.
 */
SoftwareRenderer *SoftwareRenderer::Create(SDL_Window *window) {
  // Create and verify the surface.
  SDL_Surface *render_surface = SDL_CreateRGBSurface(0, NES_WIDTH, NES_HEIGHT,
                                PALETTE_DEPTH, PALETTE_RMASK, PALETTE_GMASK,
                                PALETTE_BMASK, 0);
  if (render_surface == NULL) { return NULL; }

  // Disable RLE acceleration on the render surface.
  SDL_SetSurfaceRLE(render_surface, 0);

  // Use the rendering surface to create a SoftwareRenderer object.
  return new SoftwareRenderer(window, render_surface);
}

/*
 * Creates a SoftwareRenderer object using the given window and rendering
 * surface.
 *
 * Assumes the window and rendering surface are valid.
 */
SoftwareRenderer::SoftwareRenderer(SDL_Window *window, SDL_Surface *surface)
                                                         : Renderer(window) {
  render_surface_ = surface;
  return;
}

/*
 * Draws the given pixel to the rendering surface.
 *
 * Assumes the SDL window and rendering surface are valid.
 * Assumes that the row and column are in range of the surface size.
 */
void SoftwareRenderer::DrawPixels(size_t row, size_t col,
                                  Pixel *pixels, size_t num) {
  CONTRACT(row < static_cast<size_t>(NES_HEIGHT));
  CONTRACT(col < static_cast<size_t>(NES_WIDTH));
  CONTRACT((row * NES_WIDTH + col + num) < (NES_WIDTH * NES_HEIGHT));

  // Draw the pixels to the given location in the rendering surface.
  size_t index = row * NES_WIDTH + col;
  uint32_t *pixel_surface = static_cast<uint32_t*>(render_surface_->pixels);
  for (size_t i = 0; i < num; i++) {
    pixel_surface[index + i] = pixels[i];
  }

  return;
}

/*
 * Copies the rendering surface to the window.
 *
 * Assumes that the SDL window is valid.
 */
void SoftwareRenderer::DrawFrame(void) {
  // Get the window surface, and recalculate the rect, if the surface is invalid.
  if (!window_size_valid_) {
    window_surface_ = SDL_GetWindowSurface(window_);
    GetWindowRect();
    SDL_FillRect(window_surface_, NULL, 0);
    window_size_valid_ = true;
  }

  // Copy the render surface to the window surface.
  SDL_BlitScaled(render_surface_, &kFrameRect_, window_surface_, &window_rect_);

  // Draw the frame to the window.
  SDL_UpdateWindowSurface(window_);

  return;
}

/*
 * Frees the rendering surface.
 */
SoftwareRenderer::~SoftwareRenderer(void) {
  SDL_FreeSurface(render_surface_);
  return;
}
