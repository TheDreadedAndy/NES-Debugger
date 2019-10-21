#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"

#ifndef _NES_VID
#define _NES_VID

// The types of rendering supported by the emulator.
typedef enum { RENDER_SOFTWARE, RENDER_HARDWARE } RenderType;

/*
 * Abstract rendering class, used by the emulation to draw the game to
 * the window.
 */
class Render {
  private:
    // Holds a pointer to the current SDL window.
    SDL_Window *window_;

    // Set when a frame is drawn. Reset when HasDrawn() is called.
    // Used to track the frame rate and time the emulator.
    bool frame_output_;

    // Set by the event manager when the size of the window changes.
    bool window_size_valid_;

    // Determines the dimensions of the window rect for correct scaling
    // of the NES picture.
    void GetWindowRect(SDL_Rect *window_rect);

    Render(SDL_Window *window);

  public:
    // Creates the specified renderer, and returns it cast to a Render class.
    Render *Create(SDL_Window *window, RenderType type);

    // Draws a pixel to the window. The pixel will not be shown until Frame()
    // is called.
    virtual void Pixel(size_t row, size_t col, uint32_t pixel) = 0;

    // Renders any pixel changes to the main window.
    virtual void Frame(void) = 0;

    // Returns if the frame has been drawn since the last call to this function.
    bool HasDrawn(void);

    // Signals that the window surface must be obtained again.
    void InvalidateWindowSurface(void);

    // Declared as virtual to allows the derived renderers destructor to be
    // called when this object is deleted.
    virtual ~Render(void) = 0;
};

#endif
