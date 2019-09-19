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
 * Set whenever the ppu draws a frame (which happens at vblank).
 * Reset whenever render_has_drawn() is called.
 * Used to track the frame rate of the emulator and, thus, throttle it.
 */
static bool frame_output = false;

/*
 * Set by the event manager whenever the size of the window changes.
 */
static bool window_size_valid = false;

/*
 * Holds the next frame of pixels to be streamed to the texture and rendered
 * to the window.
 */
static uint32_t *pixels = NULL;

/*
 * Used to stream the pixel changes to the window renderer.
 */
static SDL_Texture *frame_texture = NULL;

/* Helper functions */
void render_get_window_rect(SDL_Rect *window_rect);

/*
 * Allocates the buffer used to render a frame of video.
 */
void render_init(void) {
  pixels = xcalloc(NES_WIDTH * NES_HEIGHT, sizeof(uint32_t));
  frame_texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGB888,
                  SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
  return;
}

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, uint32_t pixel) {
  CONTRACT(row < (size_t) NES_HEIGHT);
  CONTRACT(col < (size_t) NES_WIDTH);

  // Write the given pixel to the given location.
  pixels[row * NES_WIDTH + col] = pixel;

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

  // Setup the rendering rects.
  static SDL_Rect frame_rect = { .x = NES_WIDTH_OFFSET, .y = NES_HEIGHT_OFFSET,
                                 .w = NES_WIDTH, .h = NES_TRUE_HEIGHT };
  static SDL_Rect window_rect;

  // Recalculate the window rect if the window has been resized.
  // Clear the window on resize.
  if (!window_size_valid) {
    render_get_window_rect(&window_rect);
    window_size_valid = true;
    SDL_SetRenderDrawColor(render, 0, 0, 0, 0);
    SDL_RenderClear(render);
  }

  // Update the texture with the latest pixel data.
  void *texture_pixels;
  int pitch;
  SDL_LockTexture(frame_texture, NULL, &texture_pixels, &pitch);
  for (int i = 0; i < (NES_WIDTH * NES_HEIGHT); i++) {
    ((uint32_t*) texture_pixels)[i] = pixels[i];
  }
  SDL_UnlockTexture(frame_texture);

  // Copy the render surface to the window surface.
  SDL_RenderCopy(render, frame_texture, &frame_rect, &window_rect);

  // Update the window.
  SDL_RenderPresent(render);

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
void render_get_window_rect(SDL_Rect *window_rect) {
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
 * Frees the pixel buffer that was created when render_init() was called.
 */
void render_free(void) {
  free(pixels);
  SDL_DestroyTexture(frame_texture);
  return;
}
