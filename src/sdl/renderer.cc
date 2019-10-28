/*
 * TODO
 */

#include "./renderer.h"

#include <cstdlib>

#include <SDL2/SDL.h>

#include "./window.h"
#include "./hardware_renderer.h"
#include "./software_renderer.h"

/*
 * Attempts to create the requested rendering system.
 *
 * Returns NULL on failure.
 */
Renderer *Renderer::Create(SDL_Window *window, RenderType type) {
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
Renderer::Renderer(SDL_Window *window) {
  window_ = window;
  frame_output_ = false;
  window_size_valid_ = false;
  frame_rect_.x = NES_WIDTH_OFFSET;
  frame_rect_.y = NES_HEIGHT_OFFSET;
  frame_rect_.w = NES_WIDTH;
  frame_rect_.h = NES_TRUE_HEIGHT;
  return;
}

/*
 * Determines what the size of the window rect should be in order to
 * properly scale the NES picture to the window.
 *
 * Assumes the given surface and rect are non-null.
 */
void Renderer::GetWindowRect(void) {
  // Get the size of the window, to be used in the following calculation.
  int w, h;
  SDL_GetWindowSize(window_, &w, &h);

  // Determine which dimension the destination window should be padded in.
  if ((NES_TRUE_H_TO_W * w) > h) {
    // Fill in height, pad in width.
    window_rect_.h = h;
    window_rect_.y = 0;
    window_rect_.w = NES_W_TO_H * window_rect_.h;
    window_rect_.x = (w / 2) - (window_rect_.w / 2);
  } else {
    // Fill in width, pad in height.
    window_rect_.w = NES_TRUE_WIDTH_RATIO * w;
    window_rect_.x = NES_WIDTH_PAD_OFFSET_RATIO * window_rect_.w;
    window_rect_.h = NES_TRUE_H_TO_W * w;
    window_rect_.y = (h / 2) - (window_rect_.h / 2);
  }

  return;
}

/*
 * Returns the current value of frame_output_, sets frame_output to false.
 * Used to track frame timing in the emulation.
 */
bool Renderer::HasDrawn(void) {
  bool frame = frame_output_;
  frame_output_ = false;
  return frame;
}

/*
 * Sets the value of window_size_valid_ to false.
 * Called from the SDL event manager to invalidate the window.
 */
void Renderer::InvalidateWindowSurface(void) {
  window_size_valid_ = false;
  return;
}

/*
 * Prevents issues with freeing an object of type Render.
 */
Renderer::~Renderer(void) {
  return;
}
