/*
 * TODO
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "./window.h"
#include "./renderer.h"
#include "./hardware_renderer.h"
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
  pixel_buffer_ = new uint32_t[NES_WIDTH * NES_HEIGHT];
  frame_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB888,
                   SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);

  return;
}

/*
 * Draws a pixel to the pixel buffer.
 *
 * Assumes the row and column are in range of the buffer size.
 */
void HardwareRenderer::Pixel(size_t row, size_t col, uint32_t pixel) {
  CONTRACT(row < static_cast<size_t>(NES_HEIGHT));
  CONTRACT(col < static_cast<size_t>(NES_WIDTH));

  // Write the given pixel to the given location.
  pixel_buffer_[row * NES_WIDTH + col] = pixel;

  return;
}

/*
 * Renders the pixel buffer to the screen using hardware accelaration.
 *
 * Assumes that the window provided during initialization was valid.
 */
void HardwareRenderer::Frame(void) {
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
  SDL_RenderCopy(renderer_, frame_texture_, &frame_rect_, &window_rect_);

  // Update the window.
  SDL_RenderPresent(renderer_);

  // Signal that a frame was drawn.
  frame_output_ = true;

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
