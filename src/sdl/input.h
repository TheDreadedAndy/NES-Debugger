#include <stdlib.h>
#include <SDL2/SDL.h>

#ifndef _NES_INPUT
#define _NES_INPUT

// Loads the configuration file and sets up the controller mapping.
void input_load(char *file);

// Presses the given key if it is mapped to the emulation.
void input_press(SDL_Keycode key);

// Releases the given key if it is mapped to the emulation.
void input_release(SDL_Keycode key);

#endif
