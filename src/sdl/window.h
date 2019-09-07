#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#ifndef _NES_SDL
#define _NES_SDL

// Main window and rendering surface, used by render.c.
extern SDL_Window *window;
extern SDL_Surface *render;

// Creates the SDL window.
bool window_init(void);

// Processes all relevent events on the SDL event queue.
void window_process_events(void);

// Displays the given FPS in the main window title.
void window_display_fps(double fps);

// Closes the SDL window.
void window_close(void);

#endif
