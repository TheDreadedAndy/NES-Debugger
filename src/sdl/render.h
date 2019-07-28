#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"

#ifndef _NES_VID
#define _NES_VID

// Draws a pixel to the window. The pixel will not be shown until
// render_frame() is called.
void render_pixel(size_t row, size_t col, word_t pixel);

// Renders any pixel changes to the main window.
void render_frame(void);

// Returns if a frame has been drawn since the last call of this function.
bool render_has_drawn(void);

#endif
