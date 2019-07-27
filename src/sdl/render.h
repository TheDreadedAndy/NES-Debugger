#include <stdlib.h>

#ifndef _NES_VID
#define _NES_VID

// Draws a pixel to the window. The pixel will not be shown until
// render_frame() is called.
void render_pixel(size_t row, size_t col, word_t pixel);

// Renders any pixel changes to the main window.
void render_frame(void);

#endif
