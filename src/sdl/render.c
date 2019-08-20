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
#include "../ppu/ppu.h"

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
bool frame_output = false;

/* Helper functions */
void render_get_render_rect(SDL_Rect *render_rect);

/*
 * Draws a pixel to the render surface.
 *
 * Assumes the render surface has been initialized.
 * Assumes the row and column are in range of the surface size.
 */
void render_pixel(size_t row, size_t col, word_t pixel) {
  CONTRACT(row < (size_t) next_frame->h);
  CONTRACT(col < (size_t) next_frame->w);

  // Write the given pixel to the given location.
  uint32_t *pixels = (uint32_t*) next_frame->pixels;
  pixels[row * next_frame->w + col] = palette_decode(pixel);

  return;
}

/*
 * Copies the rendering surface to the window.
 *
 * Assumes the window and render surface have been initialized.
 */
void render_frame(void) {
  CONTRACT(window != NULL);
  CONTRACT(next_frame != NULL);

  // Get the regions of the surfaces to be copied.
  SDL_Rect next_frame_rect, render_rect;
  next_frame_rect.x = NES_WIDTH_OFFSET;
  next_frame_rect.y = NES_HEIGHT_OFFSET;
  next_frame_rect.w = NES_WIDTH;
  next_frame_rect.h = NES_TRUE_HEIGHT;
  render_get_render_rect(&render_rect);

  // Clear the window surface before displaying the new image.
  uint32_t background_color = palette_decode(fill_color);
  word_t red = (background_color & PALETTE_RMASK) >> 16;
  word_t green = (background_color & PALETTE_GMASK) >> 8;
  word_t blue = (background_color & PALETTE_BMASK);
  SDL_SetRenderDrawColor(render, red, green, blue, 0);
  SDL_RenderClear(render);

  // Create the texture to be rendered.
  SDL_Texture *texture = SDL_CreateTextureFromSurface(render, next_frame);

  // Render the frame and update the window.
  SDL_RenderCopy(render, texture, &next_frame_rect, &render_rect);
  SDL_RenderPresent(render);

  // Free the frame that was drawn.
  SDL_DestroyTexture(texture);

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
void render_get_render_rect(SDL_Rect *render_rect) {
  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  // Determine which dimension the destination window should be padded in.
  if ((NES_TRUE_H_TO_W * w) > h) {
    // Fill in height, pad in width.
    render_rect->h = h;
    render_rect->y = 0;
    render_rect->w = NES_W_TO_H * render_rect->h;
    render_rect->x = (w / 2) - (render_rect->w / 2);
  } else {
    // Fill in width, pad in height.
    render_rect->w = NES_TRUE_WIDTH_RATIO * w;
    render_rect->x = NES_WIDTH_PAD_OFFSET_RATIO * render_rect->w;
    render_rect->h = NES_TRUE_H_TO_W * w;
    render_rect->y = (h / 2) - (render_rect->h / 2);
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
