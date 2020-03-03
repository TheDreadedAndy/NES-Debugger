/*
 * Implements a renderer class using SDL's texture and hardware rendering
 * systems. This implementation is faster than software rendering
 * on systems with decent OpenGL support.
 *
 * While a hardware renderer can be created directly, it is intended that
 * all renderers are created through a call to Renderer::Create(), with
 * the rendering system selected through a RenderType enum.
 */

#include "./hardware_renderer.h"

#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include <SDL2/SDL.h>

#include "./window.h"
#include "./renderer.h"
#include "../util/contracts.h"

/*
 * Attempts to create a hardware rendering object. Returns NULL on failure.
 */
HardwareRenderer *HardwareRenderer::Create(SDL_Window *window) {
  // Ensure that the provided window is non-null.
  if (window == NULL) { return NULL; }

  // Attempt to create a hardware renderer.
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                           SDL_RENDERER_ACCELERATED);

  // Verify that the renderer was created successfully.
  if (renderer == NULL) { return NULL; }

  // Return the HardwareRenderer object.
  return new HardwareRenderer(window, renderer);
}

/*
 * Uses the provided SDL renderer to create a hardware rendering object.
 */
HardwareRenderer::HardwareRenderer(SDL_Window *window, SDL_Renderer *renderer)
                : Renderer(window) {
  // Store the provided renderer.
  renderer_ = renderer;

  // Allocate the rendering buffers.
  pixel_buffer_ = new uint32_t[NES_WIDTH * NES_HEIGHT]();
  frame_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB888,
                   SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);

  return;
}

/*
 * Draws the given array of pixels to the given location in the pixel
 * buffer.
 *
 * Assumes the row and column are in range of the buffer size.
 * Assumes the number of pixels will fit within the bounds of the specified
 * location.
 */
void HardwareRenderer::DrawPixels(size_t row, size_t col,
                                  Pixel *pixels, size_t num) {
  CONTRACT(row < static_cast<size_t>(NES_HEIGHT));
  CONTRACT(col < static_cast<size_t>(NES_WIDTH));
  CONTRACT((row * NES_WIDTH + col + num) < (NES_WIDTH * NES_HEIGHT));

  // Draw the given pixels to the given location.
  size_t index = row * NES_WIDTH + col;
  for (size_t i = 0; i < num; i++) {
    pixel_buffer_[index + i] = pixels[i];
  }

  return;
}

/*
 * Renders the pixel buffer to the screen using hardware accelaration.
 *
 * Assumes that the window provided during initialization was valid.
 */
void HardwareRenderer::DrawFrame(void) {
  // Recalculate the window rect if the window has been resized.
  // Clear the window on resize.
  if (!window_size_valid_) {
    GetWindowRect();
    window_size_valid_ = true;
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
  }

  // Update the texture with the latest pixel data.
  void *texture_pixels;
  int pitch;
  SDL_LockTexture(frame_texture_, NULL, &texture_pixels, &pitch);
  for (int i = 0; i < (NES_WIDTH * NES_HEIGHT); i++) {
    (static_cast<uint32_t*>(texture_pixels))[i] = pixel_buffer_[i];
  }
  SDL_UnlockTexture(frame_texture_);

  // Copy the render surface to the window surface.
  SDL_RenderCopy(renderer_, frame_texture_, &kFrameRect_, &window_rect_);

  // Update the window.
  SDL_RenderPresent(renderer_);

  return;
}

/*
 * Frees the pixel buffer, frame texture, and renderer used for hardware
 * accelerated rendering.
 */
HardwareRenderer::~HardwareRenderer(void) {
  // Free all hardware rendering structures and buffers.
  delete[] pixel_buffer_;
  SDL_DestroyTexture(frame_texture_);
  SDL_DestroyRenderer(renderer_);
  return;
}
