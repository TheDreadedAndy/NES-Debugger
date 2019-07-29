/*
 * This file holds function to interact with SDL and collect input from the
 * user. It then provides this input to the emulator in a form independent
 * of SDL.
 *
 * Additionally, this file creates/loads a configuration file which can be
 * changed to remap the NES controller buttons.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <SDL/SDL2.h>
#include "./window.h"
#include "../util/util.h"
#include "../util/contracts.h"

/*
 * These strings represent configuration options which can be used to change
 * the input map.
 */
const char *button_a = "BUTTON_A"
const char *button_b = "BUTTON_B"
const char *button_start = "BUTTON_START"
const char *button_select = "BUTTON_SELECT"
const char *pad_up = "PAD_UP"
const char *pad_down = "PAD_DOWN"
const char *pad_left = "PAD_LEFT"
const char *pad_right = "PAD_RIGHT"

/*
 * These variables hold the current/default mapping for the NES controller.
 */
SDL_Keysym map_a = SDLK_X;
SDL_Keysys map_b = SDLK_Z;
SDL_Keysym map_start = SDLK_RETURN;
SDL_Keysym map_select = SDLK_BACKSPACE;
SDL_Keysym map_up = SDLK_UP;
SDL_Keysym map_down = SDLK_DOWN;
SDL_Keysym map_left = SDLK_LEFT;
SDL_Keysym map_right = SDLK_RIGHT;

/*
 * The name of the default configuration file.
 */
const char *default_config = "ndb.cfg"

/*
 * TODO
 */
void input_load(char *file) {
  // Load the default file if none was provided.
  if (file == NULL) { file = default_config; }

  // Check if the file is empty, and create the config if it is.
  input_create_config(file);

  // Otherwise, attempt to load the config.
  input_load_config(file);

  return;
}

/*
 * TODO
 */
void input_press(SDL_Keysym key) {
  (void) key;
  return;
}

/*
 * TODO
 */
void input_release(SDL_Keysym key) {
  (void) key;
  return;
}
