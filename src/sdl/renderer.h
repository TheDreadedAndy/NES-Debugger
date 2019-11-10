#ifndef _NES_VID
#define _NES_VID

#include <cstdio>

#include <SDL2/SDL.h>

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

// The types of rendering supported by the emulator.
typedef enum { RENDER_SOFTWARE, RENDER_HARDWARE } RenderType;

/*
 * Abstract rendering class, used by the emulation to draw the game to
 * the window.
 */
class Renderer {
  protected:
    // Used to scale the output to the window.
    const SDL_Rect kFrameRect_ = { NES_WIDTH_OFFSET, NES_HEIGHT_OFFSET,
                                   NES_WIDTH, NES_TRUE_HEIGHT };
    SDL_Rect window_rect_;

    // Holds a pointer to the current SDL window.
    SDL_Window *window_;

    // Set by the event manager when the size of the window changes.
    bool window_size_valid_ = false;

    // Determines the dimensions of the window rect for correct scaling
    // of the NES picture.
    void GetWindowRect(void);

    Renderer(SDL_Window *window);

  public:
    // Creates the specified renderer, and returns it cast to a Render class.
    static Renderer *Create(SDL_Window *window, RenderType type);

    // Draws a pixel to the window. The pixel will not be shown until Frame()
    // is called.
    virtual void Pixel(size_t row, size_t col, uint32_t pixel) = 0;

    // Renders any pixel changes to the main window.
    virtual void Frame(void) = 0;

    // Signals that the window surface must be obtained again.
    void InvalidateWindowSurface(void);

    // Declared as virtual to allows the derived renderers destructor to be
    // called when this object is deleted.
    virtual ~Renderer(void) = 0;
};

#endif
