#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "./renderer.h"

#ifndef _NES_HWRENDER
#define _NES_HWRENDER

/*
 * Hardware rendering implementation of a Render class.
 */
class HardwareRenderer : public Renderer {
  private:
    // Used to scale the output to the window.
    SDL_Rect frame_rect_;
    SDL_Rect window_rect_;

    // Holds the next frame of pixel data to be streamed to the texture.
    uint32_t *pixel_buffer_;

    // Used to stream pixel changes to the window renderer.
    SDL_Texture *frame_texture_;

    // The SDL hardware renderer tied to the window.
    SDL_Renderer *renderer_;

    // Uses the provided renderer to create a HardwareRenderer object.
    HardwareRenderer(SDL_Window *window, SDL_Renderer *renderer);

  public:
    // Functions implemented from the abstract class.
    void Pixel(size_t row, size_t col, uint32_t pixel);
    void Frame(void);

    // Attempts to create a HardwareRenderer object. Returns NULL on failure.
    HardwareRenderer *Create(SDL_Window *window);

    // Frees the structures and buffers related to this class.
    ~HardwareRenderer(void);
};

#endif
