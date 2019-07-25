#include <stdio.h>
#include <SDL2/SDL.h>

#ifndef _NES_SDL
#define _NES_SDL

// Creates the SDL window.
bool window_init(void);

// Draws a pixel to the window. The pixel will not be shown until
// window_draw_frame() is called.
void window_draw_pixel(size_t row, size_t col, word_t pixel);

// Renders any pixel changes to the main window.
void window_draw_frame(void);

// Closes the SDL window.
void window_close(void);

#endif
