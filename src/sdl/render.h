#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"

#ifndef _NES_VID
#define _NES_VID

// Initializes rendering.
void render_init(void);

// Draws a pixel to the window. The pixel will not be shown until
// render_frame() is called.
void render_pixel(size_t row, size_t col, uint32_t pixel);

// Renders any pixel changes to the main window.
void render_frame(void);

// Returns if a frame has been drawn since the last call of this function.
bool render_has_drawn(void);

// Signals that the window surface must be obtained again.
void render_invalidate_window_surface(void);

// Closes rendering.
void render_free(void);

#endif
