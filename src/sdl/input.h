#ifndef _NES_INPUT
#define _NES_INPUT

#include <SDL2/SDL.h>

#include "../util/data.h"
#include "../config/config.h"

// The number of buttons on the NES controller.
#define NUM_BUTTONS 8

/*
 * Translates SDL key presses into button presses. These button presses
 * can then be polled and used by the emulator. The mapping for the
 * SDL key presses is loaded from the given config file.
 */
class Input {
  private:
    // The keys for each button in configuration file of the emulation.
    const char *kButtonNames_[NUM_BUTTONS] = { kButtonAKey, kButtonBKey,
                kButtonSelectKey, kButtonStartKey, kButtonUpKey, kButtonDownKey,
                kButtonLeftKey, kButtonRightKey };

    // The default button mappings for the emulator.
    // Note that the order of this array must match the one above.
    const SDL_Keycode kDefaultButtonMap_[NUM_BUTTONS] = { SDLK_x, SDLK_z,
          SDLK_BACKSPACE, SDLK_RETURN, SDLK_UP, SDLK_DOWN, SDLK_LEFT,
          SDLK_RIGHT };

    // Holds the current button mapping for the controller.
    // Note that the order of this array must match the ones above.
    SDL_Keycode button_map_[NUM_BUTTONS];

    // Holds the current pressed/released state for each button.
    DataWord input_status_ = 0;

    // Used to prevent the play from pressing opposing dpad directions
    // at the same time.
    bool dpad_priority_up_ = false;
    bool dpad_priority_left_ = false;

  public:
    // Loads the given config file, or a default if none is specified.
    Input(Config *config);

    // Presses the given key, if it's mapped.
    void Press(SDL_Keycode key);

    // Releases the given key, if it's mapped.
    void Release(SDL_Keycode key);

    // Returns a byte containing the current valid button presses.
    DataWord Poll(void);
};

#endif
