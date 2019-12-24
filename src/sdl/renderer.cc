/*
 * The renderer abstract class define an interface which can be used
 * by the PPU emulation to draw pixels to the main window. It abstracts
 * window resizing, scaling, and the use of SDL away from the emulation.
 * This allows for code that is clean and clear, and enables the rendering
 * method to be changed without changing the PPU emulation itself.
 *
 * Note that the SDL event manager, defined in the file sdl/window.cc, must
 * signal the renderer object whenever the window size changes, or else
 * scaling will not be updated properly.
 *
 * There are currently two renderer implementations: software and hardware.
 * The software renderer uses SDL surfaces and is faster on hardware with
 * poor OpenGL support. The hardware renderer uses SDL renderers/textures, and
 * is faster on hardware which supports OpenGL well.
 */

#include "./renderer.h"

#include <cstdlib>

#include <SDL2/SDL.h>

#include "../config/config.h"
#include "../util/util.h"
#include "./window.h"
#include "./hardware_renderer.h"
#include "./software_renderer.h"

/*
 * Attempts to create the requested rendering system.
 *
 * Returns NULL on failure.
 */
Renderer *Renderer::Create(SDL_Window *window, Config *config) {
  // Calls the creation function for the appropriate derived class.
  char *type = config->Get(kRendererTypeKey, kRendererHardwareVal);
  if (StrEq(type, kRendererSurfaceVal)) {
    return SoftwareRenderer::Create(window);
  } else if (StrEq(type, kRendererHardwareVal)) {
    return HardwareRenderer::Create(window);
  } else {
    return NULL;
  }
}

/*
 * Initializes the base rendering variables.
 */
Renderer::Renderer(SDL_Window *window) {
  window_ = window;
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
