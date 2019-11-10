/*
 * This file provides an interface for the emulator to use to
 * communicate with SDL, allowing for controller emulation
 * that is independent of the input method/implementation.
 *
 * Configuration files can be specified when creating an input object,
 * and allow the user to rebind the keys which correspond to each
 * button on the NES controller.
 *
 * The input class provides a method for the controller to poll for input.
 * When the poll function is called, the input object returns a DataWord
 * corresponding to the currently pressed buttons. Note that impossible
 * inputs are masked out of this word (Ex. down + up at the same time).
 */

#include "./input.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <SDL2/SDL.h>

#include "./window.h"
#include "../util/util.h"
#include "../util/contracts.h"
#include "../util/data.h"

// Map array access constants.
#define MAP_A 0
#define MAP_B 1
#define MAP_SELECT 2
#define MAP_START 3
#define MAP_UP 4
#define MAP_DOWN 5
#define MAP_LEFT 6
#define MAP_RIGHT 7
#define NUM_BUTTONS 8

// Input status is stored in an 8-bit word. These flags mask the bits
// that correspond to each button.
#define FLAG_A 0x01U
#define FLAG_B 0x02U
#define FLAG_SELECT 0x04U
#define FLAG_START 0x08U
#define FLAG_UP 0x10U
#define FLAG_DOWN 0x20U
#define FLAG_LEFT 0x40U
#define FLAG_RIGHT 0x80U

// The max size of the buffers used to read in parts of the file.
#define BUFFER_MAX 256

/*
 * Loads the input mapping, allowing for key presses and releases to be used
 * for emulation.
 */
Input::Input(char *file) {
  // Load in the default button mapping.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button_map_[i] = kDefaultButtonMap_[i];
  }

  // Load the default file if none was provided.
  FILE *config;
  if (file == NULL) {
    config = fopen(kDefaultConfig_, "a+");
  } else {
    config = fopen(file, "a+");
  }

  // Check if the file is empty, and create the config if it is.
  if (GetFileSize(config) == 0) {
    CreateConfig(config);
  } else {
    // Attempt to load the config file into the button mappings.
    LoadConfig(config);
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
void Input::CreateConfig(FILE *config) {
  CONTRACT(config != NULL);

  // Write the default mapping for each button.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    fwrite(kButtonNames_[i], 1, strlen(kButtonNames_[i]), config);
    fwrite("=", 1, 1, config);
    fwrite(SDL_GetKeyName(button_map_[i]), 1,
           strlen(SDL_GetKeyName(button_map_[i])), config);
    fwrite("\n", 1, 1, config);
  }

  return;
}

/*
 * Load the button mapping from the provided file stream.
 *
 * Assumes the provided file is open and valid.
 */
void Input::LoadConfig(FILE *config) {
  CONTRACT(config != NULL);

  // Reset the file to the begining.
  fseek(config, 0, SEEK_SET);

  // Read the mappings on each line of the config.
  char cfg_buf[BUFFER_MAX];
  char key_buf[BUFFER_MAX];
  while (fgetc(config) != EOF) {
    // Undo the stream position increment from the loop guard.
    fseek(config, -1, SEEK_CUR);

    // Read in the configuration name string and verify it was successful.
    if (!FileReadChunk(cfg_buf, config, BUFFER_MAX, '=')) {
      continue;
    }

    // Read in the key name string and verify it was successful.
    if (!FileReadChunk(key_buf, config, BUFFER_MAX, '\n')) {
      continue;
    }

    // Update the mapping using the read in strings.
    SetMap(cfg_buf, key_buf);
  }

  return;
}

/*
 * Reads up until the specified char or max size into an array from a file.
 * Returns true when the string was read successfully.
 *
 * Assumes the array and file are non-null.
 */
bool Input::FileReadChunk(char *buffer, FILE *file, int max_size, char term) {
  CONTRACT(buffer != NULL && file != NULL);

  // Read from the file into the buffer until a termination byte is reached
  // or the buffer is full.
  int size = 0;
  int next_byte = fgetc(file);
  while ((size < (max_size - 1)) && (next_byte != term) && (next_byte != EOF)) {
    buffer[size] = static_cast<char>(next_byte);
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
 * Assumes the strings are non-null.
 * Assumes SDL has been initialized.
 */
void Input::SetMap(char *cfg_name, char *key_name) {
  CONTRACT(cfg_name != NULL && key_name != NULL);

  // Get the map index of the specified config string, if it exists.
  size_t button = 0;
  while (strcmp(cfg_name, kButtonNames_[button])) {
    button++;
    // If the config string is invalid, so we return.
    if (button >= NUM_BUTTONS) { return; }
  }

  // Get the SDL key and write it to the mapping if it is valid.
  SDL_Keycode key = SDL_GetKeyFromName(key_name);
  if (key != SDLK_UNKNOWN) { button_map_[button] = key; }

  return;
}

/*
 * Determines if the given keycode belongs to any button in the mapping,
 * presses that button if it does.
 *
 * Assumes the mapping has been initialized.
 * Assumes SDL has been initialized.
 */
void Input::Press(SDL_Keycode key) {
  // Determine which button was pressed, if any.
  size_t button = 0;
  while ((button < NUM_BUTTONS) && (button_map_[button] != key)) { button++; }

  // If the button is not in the map, we do nothing.
  if (button >= NUM_BUTTONS) { return; }

  // Otherwise, we update the pressed button in the input status.
  switch(button) {
    case MAP_A:
      input_status_ |= FLAG_A;
      break;
    case MAP_B:
      input_status_ |= FLAG_B;
      break;
    case MAP_SELECT:
      input_status_ |= FLAG_SELECT;
      break;
    case MAP_START:
      input_status_ |= FLAG_START;
      break;
    case MAP_UP:
      dpad_priority_up_ = true;
      input_status_ |= FLAG_UP;
      break;
    case MAP_DOWN:
      dpad_priority_up_ = false;
      input_status_ |= FLAG_DOWN;
      break;
    case MAP_LEFT:
      dpad_priority_left_ = true;
      input_status_ |= FLAG_LEFT;
      break;
    case MAP_RIGHT:
      dpad_priority_left_ = false;
      input_status_ |= FLAG_RIGHT;
      break;
    default:
      break;
  }

  return;
}

/*
 * Determines if the given keycode belongs to any button in the mapping,
 * releases that button if it does.
 *
 * Assumes the mapping has been initialized.
 * Assumes SDL has been initialized.
 */
void Input::Release(SDL_Keycode key) {
  // Determine which button was released, if any.
  size_t button = 0;
  while ((button < NUM_BUTTONS) && (button_map_[button] != key)) { button++; }

  // If the button is not in the map, we do nothing.
  if (button >= NUM_BUTTONS) { return; }

  // Otherwise, we update the released button in the input status.
  switch(button) {
    case MAP_A:
      input_status_ &= ~FLAG_A;
      break;
    case MAP_B:
      input_status_ &= ~FLAG_B;
      break;
    case MAP_SELECT:
      input_status_ &= ~FLAG_SELECT;
      break;
    case MAP_START:
      input_status_ &= ~FLAG_START;
      break;
    case MAP_UP:
      dpad_priority_up_ = false;
      input_status_ &= ~FLAG_UP;
      break;
    case MAP_DOWN:
      dpad_priority_up_ = true;
      input_status_ &= ~FLAG_DOWN;
      break;
    case MAP_LEFT:
      dpad_priority_left_ = false;
      input_status_ &= ~FLAG_LEFT;
      break;
    case MAP_RIGHT:
      dpad_priority_left_ = true;
      input_status_ &= ~FLAG_RIGHT;
      break;
    default:
      break;
  }

  return;
}

/*
 * Returns a byte that contains the current set of valid controller inputs.
 * Conflicting directions in the input status are masked out based on which
 * was pressed more recently.
 */
DataWord Input::Poll(void) {
  DataWord vmask = (dpad_priority_up_) ? (~FLAG_DOWN) : (~FLAG_UP);
  DataWord hmask = (dpad_priority_left_) ? (~FLAG_RIGHT) : (~FLAG_LEFT);
  return input_status_ & vmask & hmask;
}
