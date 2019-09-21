#include <stdlib.h>
#include <stdbool.h>
#include "../util/data.h"

#ifndef _NES_VID
#define _NES_VID

// Function definition for rendering functions.
typedef void render_free_t(void);
typedef void render_pixel_t(size_t row, size_t col, uint32_t pixel);
typedef void render_frame_t(void);

// Holds the functions to access the in-use implementation of rendering.
typedef struct render {
  render_free_t *free;
  render_pixel_t *pixel;
  render_frame_t *frame;
} render_t;

// Exposes the in-use rendering implementation to the user.
extern render_t *render;

// Initializes rendering.
bool render_init(bool use_surface_rendering);

// Draws a pixel to the window. The pixel will not be shown until
// render_frame() is called.
void render_pixel_surface(size_t row, size_t col, uint32_t pixel);
void render_pixel_hardware(size_t row, size_t col, uint32_t pixel);

// Renders any pixel changes to the main window.
void render_frame_surface(void);
void render_frame_hardware(void);

// Returns if a frame has been drawn since the last call of this function.
bool render_has_drawn(void);

// Signals that the window surface must be obtained again.
void render_invalidate_window_surface(void);

// Closes rendering.
void render_free_surface(void);
void render_free_hardware(void);

#endif
