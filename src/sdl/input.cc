/*
 * This file provides an interface for the emulator to use to
 * communicate with SDL, allowing for controller emulation
 * that is independent of the input method/implementation.
 *
 * The mapping for the emulated controller can be changed within the
 * configuration file.
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
#include "../config/config.h"

// Map array access constants.
#define MAP_A 0
#define MAP_B 1
#define MAP_SELECT 2
#define MAP_START 3
#define MAP_UP 4
#define MAP_DOWN 5
#define MAP_LEFT 6
#define MAP_RIGHT 7

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

/*
 * Loads the input mapping, allowing for key presses and releases to be used
 * for emulation.
 */
Input::Input(Config *config) {
  // Load the button mapping from the provided config file.
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button_map_[i] = SDL_GetKeyFromName(config->Get(kButtonNames_[i],
                     SDL_GetKeyName(kDefaultButtonMap_[i])));
  }
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
