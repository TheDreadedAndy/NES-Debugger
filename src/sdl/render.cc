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
 * Hardware rendering implementation of a Render class.
 */
class HardwareRenderer : public Render {
  private:
    // Holds the next frame of pixel data to be streamed to the texture.
    uint32_t *pixel_buffer_;

    // Used to stream pixel changes to the window renderer.
    SDL_Texture *frame_texture_;

    // The SDL hardware renderer tied to the window.
    SDL_Renderer *renderer_;

    // Uses the provided renderer to create a HardwareRenderer object.
    HardwareRenderer(SDL_Renderer *renderer);

  public:
    // Functions implemented from the abstract class.
    void Pixel(size_t row, size_t col, uint32_t pixel);
    void Frame(void);

    // Attempts to create a HardwareRenderer object. Returns NULL on failure.
    HardwareRenderer *Create(SDL_Window *window);

    // Frees the structures and buffers related to this class.
    ~HardwareRenderer(void);
};

/*
 * Software rendering implementation of a Render class.
 */
class SoftwareRenderer : public Render {
  private:
    // Holds the next frame to be drawn to the screen.
    SDL_Surface *render_surface_;

    // Uses the provided surface to create a SoftwareRenderer object.
    SoftwareRenderer(SDL_Surface *surface);

  public:
    // Functions implemented from the abstract class.
    void Pixel(size_t row, size_t col, uint32_t pixel);
    void Frame(void);

    // Attempts to create a SoftwareRenderer object. Returns NULL on failure.
    SoftwareRenderer *Create(SDL_Window *window);

    // Frees the surface used for software rendering.
    ~SoftwareRenderer(void);
};

/*
 * Attempts to create the requested rendering system.
 *
 * Returns NULL on failure.
 */
Render *Render::Create(SDL_Window *window, RenderType type) {
  // Calls the creation function for the appropriate derived class.
  switch (type) {
    case RENDER_SOFTWARE:
      return SoftwareRenderer::Create(window);
    case RENDER_HARDWARE:
      return HardwareRenderer::Create(window);
    default:
      break;
  }

  return NULL;
}

/*
 * Initializes the base rendering variables.
 */
Render::Render(SDL_Window *window) {
  window_ = window;
  frame_output_ = false;
  window_size_valid_ = false;
  return;
}

/*
 * Attempts to create a hardware rendering object.
 *
 * Returns NULL on failure.
 */
HardwareRenderer *HardwareRenderer::Create(SDL_Window *window) {
  // Attempt to create a hardware renderer.
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                           SDL_RENDERER_ACCELERATED);

  // Verify that the renderer was created successfully.
  if (renderer == NULL) { return NULL; }

  // Return the HardwareRenderer object.
  return new HardwareRenderer(renderer);
}

/*
 * Uses the provided SDL renderer to create a hardware rendering object.
 */
HardwareRenderer::HardwareRenderer(SDL_Renderer *renderer) {
  // Store the provided renderer.
  renderer_ = renderer;

  // Allocate the rendering buffers.
  pixel_buffer_ = new uint32_t[NES_WIDTH * NES_HEIGHT];
  frame_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB888,
                   SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);

  return;
}

/*
 * Attempts to create a software rendering object.
 *
 * Returns NULL on failure.
 */
SoftwareRenderer *SoftwareRenderer::Create(SDL_Window *window) {
  // FIXME: This is garbage code.
  if (use_surface_rendering) {
    // Create and verify the surface.
    render_surface = SDL_CreateRGBSurface(0, NES_WIDTH, NES_HEIGHT,
                     PALETTE_DEPTH, PALETTE_RMASK, PALETTE_GMASK,
                     PALETTE_BMASK, 0);
    if (render_surface == NULL) { return false; }

    // Disable RLE acceleration on the render surface.
    SDL_SetSurfaceRLE(render_surface, 0);
  } else {
      }

  // Rendering was successfully initalized.
  return true;
}

/*
 * Draws the given pixel to the rendering surface.
 *
 * Assumes that surface rendering has been initialized.
 * Assumes that the row and column are in range of the surface size.
 */
void render_pixel_surface(size_t row, size_t col, uint32_t pixel) {
  CONTRACT(row < (size_t) NES_HEIGHT);
  CONTRACT(col < (size_t) NES_WIDTH);
  CONTRACT(render_surface != NULL);

  // Write the given pixel to the given location in the rendering surface.
  uint32_t *pixels = (uint32_t*) render_surface->pixels;
  pixels[row * NES_WIDTH + col] = pixel;

  return;
}

/*
 * Draws a pixel to the pixel buffer.
 *
 * Assumes that hardware rendering has been initialized.
 * Assumes the row and column are in range of the buffer size.
 */
void render_pixel_hardware(size_t row, size_t col, uint32_t pixel) {
  CONTRACT(row < (size_t) NES_HEIGHT);
  CONTRACT(col < (size_t) NES_WIDTH);
  CONTRACT(pixel_buffer != NULL);

  // Write the given pixel to the given location.
  pixel_buffer[row * NES_WIDTH + col] = pixel;

  return;
}

/*
 * Copies the rendering surface to the window.
 *
 * Assumes the window has been initialized.
 * Assumes that software rendering has been initialized.
 */
void render_frame_surface(void) {
  CONTRACT(window != NULL);
  CONTRACT(render_surface != NULL);

  // Setup the window surface.
  static SDL_Surface *window_surface = NULL;
  static SDL_Rect render_rect = { .x = NES_WIDTH_OFFSET, .y = NES_HEIGHT_OFFSET,
                                  .w = NES_WIDTH, .h = NES_TRUE_HEIGHT };
  static SDL_Rect window_rect;

  // Get the window surface, and recalculate the rect, if the surface is invalid.
  if (!window_size_valid) {
    window_surface = SDL_GetWindowSurface(window);
    render_get_window_rect(&window_rect);
    SDL_FillRect(window_surface, NULL, 0);
    window_size_valid = true;
  }

  // Copy the render surface to the window surface.
  SDL_BlitScaled(render_surface, &render_rect, window_surface, &window_rect);

  // Update the window.
  SDL_UpdateWindowSurface(window);

  // Signal that a frame was drawn.
  frame_output = true;

  return;
}

/*
 * Renders the pixel buffer to the screen using hardware accelaration.
 *
 * Assumes the window has been initialized.
 * Assumes that rendering has been initialized with hardware acceleration.
 */
void render_frame_hardware(void) {
  CONTRACT(window != NULL);
  CONTRACT(pixel_buffer != NULL);
  CONTRACT(frame_texture != NULL);
  CONTRACT(hardware_renderer != NULL);

  // Setup the rendering rects.
  static SDL_Rect frame_rect = { .x = NES_WIDTH_OFFSET, .y = NES_HEIGHT_OFFSET,
                                 .w = NES_WIDTH, .h = NES_TRUE_HEIGHT };
  static SDL_Rect window_rect;

  // Recalculate the window rect if the window has been resized.
  // Clear the window on resize.
  if (!window_size_valid) {
    render_get_window_rect(&window_rect);
    window_size_valid = true;
    SDL_SetRenderDrawColor(hardware_renderer, 0, 0, 0, 0);
    SDL_RenderClear(hardware_renderer);
  }

  // Update the texture with the latest pixel data.
  void *texture_pixels;
  int pitch;
  SDL_LockTexture(frame_texture, NULL, &texture_pixels, &pitch);
  for (int i = 0; i < (NES_WIDTH * NES_HEIGHT); i++) {
    ((uint32_t*) texture_pixels)[i] = pixel_buffer[i];
  }
  SDL_UnlockTexture(frame_texture);

  // Copy the render surface to the window surface.
  SDL_RenderCopy(hardware_renderer, frame_texture, &frame_rect, &window_rect);

  // Update the window.
  SDL_RenderPresent(hardware_renderer);

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
static void render_get_window_rect(SDL_Rect *window_rect) {
  // Get the size of the window, to be used in the following calculation.
  int w, h;
  SDL_GetWindowSize(window, &w, &h);

  // Determine which dimension the destination window should be padded in.
  if ((NES_TRUE_H_TO_W * w) > h) {
    // Fill in height, pad in width.
    window_rect->h = h;
    window_rect->y = 0;
    window_rect->w = NES_W_TO_H * window_rect->h;
    window_rect->x = (w / 2) - (window_rect->w / 2);
  } else {
    // Fill in width, pad in height.
    window_rect->w = NES_TRUE_WIDTH_RATIO * w;
    window_rect->x = NES_WIDTH_PAD_OFFSET_RATIO * window_rect->w;
    window_rect->h = NES_TRUE_H_TO_W * w;
    window_rect->y = (h / 2) - (window_rect->h / 2);
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
 * Sets the value of window_size_valid to false.
 * Called from the SDL event manager to invalidate the window.
 */
void render_invalidate_window_surface(void) {
  window_size_valid = false;
  return;
}

/*
 * Frees the rendering surface and render structure.
 *
 * Assumes software rendering has been initialized.
 */
void render_free_surface(void) {
  CONTRACT(render_surface != NULL);
  CONTRACT(render != NULL);

  SDL_FreeSurface(render_surface);
  free(render);
  return;
}

/*
 * Frees the pixel buffer, frame texture, and renderer used for hardware
 * accelerated rendering.
 */
void render_free_hardware(void) {
  CONTRACT(pixel_buffer != NULL);
  CONTRACT(frame_texture != NULL);
  CONTRACT(hardware_renderer != NULL);
  CONTRACT(render != NULL);

  // Free all hardware rendering structures and buffers.
  free(pixel_buffer);
  SDL_DestroyTexture(frame_texture);
  SDL_DestroyRenderer(hardware_renderer);
  free(render);
  return;
}
