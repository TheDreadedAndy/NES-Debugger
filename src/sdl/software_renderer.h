#ifndef _NES_SWRENDER
#define _NES_SWRENDER

#include <cstdio>
#include <cstdint>

#include <SDL2/SDL.h>

#include "./renderer.h"
#include "../memory/palette.h"

/*
 * Software rendering implementation of a Render class.
 */
class SoftwareRenderer : public Renderer {
  private:
    // Holds the next frame to be drawn to the screen.
    SDL_Surface *render_surface_;

    // Used to draw the next frame to the screen.
    SDL_Surface *window_surface_;

    // Uses the provided surface to create a SoftwareRenderer object.
    SoftwareRenderer(SDL_Window *window, SDL_Surface *surface);

  public:
    // Functions implemented from the abstract class.
    void DrawPixels(size_t row, size_t col, DataWord *tiles, size_t num);
    void DrawFrame(void);

    // Attempts to create a SoftwareRenderer object. Returns NULL on failure.
    static SoftwareRenderer *Create(SDL_Window *window);

    // Frees the surface used for software rendering.
    ~SoftwareRenderer(void);
};

#endif
