#include <stdlib.h>
#include <SDL2/SDL.h>
#include "../util/data.h"

#ifndef _NES_INPUT
#define _NES_INPUT

// Loads the configuration file and sets up the controller mapping.
void InputLoad(char *file);

// Presses the given key if it is mapped to the emulation.
void InputPress(SDL_Keycode key);

// Releases the given key if it is mapped to the emulation.
void InputRelease(SDL_Keycode key);

// Returns a byte containing the current valid button presses.
DataWord InputPoll(void);

#endif
