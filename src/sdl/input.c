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
#include <string.h>
#include <SDL2/SDL.h>
#include "./input.h"
#include "./window.h"
#include "../util/util.h"
#include "../util/contracts.h"

// Map array access constants.
#define MAP_A 0
#define MAP_B 1
#define MAP_START 2
#define MAP_SELECT 3
#define MAP_UP 4
#define MAP_DOWN 5
#define MAP_LEFT 6
#define MAP_RIGHT 7
#define NUM_BUTTONS 8

// The max size of the buffers used to read in parts of the file.
#define BUFFER_MAX 256

/*
 * These strings represent configuration options which can be used to change
 * the input map.
 */
const char *button_names[] = { "BUTTON_A", "BUTTON_B", "BUTTON_START",
                               "BUTTON_SELECT", "PAD_UP", "PAD_DOWN",
                               "PAD_LEFT", "PAD_RIGHT" };

/*
 * These variables hold the current/default mapping for the NES controller.
 */
SDL_Keycode button_map[] = { SDLK_x, SDLK_z, SDLK_RETURN, SDLK_BACKSPACE,
                            SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };

/*
 * The name of the default configuration file.
 */
const char *default_config = "ndb.cfg";

/* Helper functions */
void input_create_config(FILE *config);
void input_load_config(FILE *config);
bool input_file_read_chunk(char *buffer, FILE *file, int max_size, char term);
void input_set_map(char *cfg, char *key);

/*
 * Loads the input mapping, allowing for key presses and releases to be used
 * for emulation.
 */
void input_load(char *file) {
  // Load the default file if none was provided.
  FILE *config;
  if (file == NULL) {
    config = fopen(default_config, "a+");
  } else {
    config = fopen(file, "a+");
  }

  // Check if the file is empty, and create the config if it is.
  if (get_file_size(config) == 0) {
    input_create_config(config);
  } else {
    // Attempt to load the config file into the button mappings.
    input_load_config(config);
  }

  // Close the file and exit.
  fclose(config);
  return;
}

/*
 * Creates an input config file from the default settings.
 *
 * Assumes the provided file is open, valid, and empty.
 */
void input_create_config(FILE *config) {
  // Write the default mapping for each button.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    fwrite(button_names[i], 1, strlen(button_names[i]), config);
    fwrite("=", 1, 1, config);
    fwrite(SDL_GetKeyName(button_map[i]), 1,
           strlen(SDL_GetKeyName(button_map[i])), config);
    fwrite("\n", 1, 1, config);
  }

  return;
}

/*
 * Load the button mapping from the provided file stream.
 *
 * Assumes the provided file is open and valid.
 */
void input_load_config(FILE *config) {
  // Reset the file to the begining.
  fseek(config, 0, SEEK_SET);

  // Read the mappings on each line of the config.
  char cfg_buf[BUFFER_MAX];
  char key_buf[BUFFER_MAX];
  while (fgetc(config) != EOF) {
    // Undo the stream position increment from the loop guard.
    fseek(config, -1, SEEK_CUR);

    // Read in the configuration name string and verify it was successful.
    if (!input_file_read_chunk(cfg_buf, config, BUFFER_MAX, '=')) {
      continue;
    }

    // Read in the key name string and verify it was successful.
    if (!input_file_read_chunk(key_buf, config, BUFFER_MAX, '\n')) {
      continue;
    }

    // Update the mapping using the read in strings.
    input_set_map(cfg_buf, key_buf);
  }

  return;
}

/*
 * Reads up until the specified char or max size into an array from a file.
 * Returns true when the string was read successfully.
 *
 * Assumes the array and file are non-null.
 */
bool input_file_read_chunk(char *buffer, FILE *file, int max_size, char term) {
  // Read from the file into the buffer until a termination byte is reached
  // or the buffer is full.
  int size = 0;
  int next_byte = fgetc(file);
  while ((size < (max_size - 1)) && (next_byte != term) && (next_byte != EOF)) {
    buffer[size] = (char) next_byte;
    size++;
    next_byte = fgetc(file);
  }
  buffer[size] = 0;

  // Verify that the buffer did not fill before the string terminated.
  if ((size >= (max_size - 1)) && (next_byte != term) && (next_byte != EOF)) {
    // Jump the file to the next line/EOF and return failure.
    do {
      next_byte = fgetc(file);
    } while ((next_byte != EOF) && (next_byte != '\n'));
    return false;
  }

  // Since the chunk was read correctly, return its size.
  return true;
}

/*
 * Map the controller button specified by the config string to the key
 * named in the key string.
 *
 * Assumes the strings are non-null with valid sizes.
 */
void input_set_map(char *cfg_name, char *key_name) {
  // Get the map index of the specified config string, if it exists.
  int button = 0;
  while (strcmp(cfg_name, button_names[button])) {
    button++;
    // If the config string is invalid, we return.
    if (button >= NUM_BUTTONS) { return; }
  }

  // Get the SDL key and write it to the mapping if it is valid.
  SDL_Keycode key = SDL_GetKeyFromName(key_name);
  if (key != SDLK_UNKNOWN) { button_map[button] = key; }

  return;
}

/*
 * TODO
 */
void input_press(SDL_Keycode key) {
  (void) key;
  return;
}

/*
 * TODO
 */
void input_release(SDL_Keycode key) {
  (void) key;
  return;
}
