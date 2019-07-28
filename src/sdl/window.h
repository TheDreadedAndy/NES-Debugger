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

// Closes the SDL window.
void window_close(void);

#endif
