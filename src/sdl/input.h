#ifndef _NES_INPUT
#define _NES_INPUT

#include <SDL2/SDL.h>

#include "../util/data.h"

// The number of buttons on the NES controller.
#define NUM_BUTTONS 8

/*
 * Holds a configuration file and button mapping, which are used to
 * translate key presses into button presses. These button presses
 * can then be polled and used by the emulator.
 */
class Input {
  private:
    // These strings represent configuration options which can be used to change
    // the input map.
    const char *kButtonNames_[8] = { "BUTTON_A", "BUTTON_B", "BUTTON_SELECT",
                                    "BUTTON_START", "PAD_UP", "PAD_DOWN",
                                    "PAD_LEFT", "PAD_RIGHT" };

    // The default mapping for the NES controller.
    const SDL_Keycode kDefaultButtonMap_[8] = { SDLK_x, SDLK_z, SDLK_BACKSPACE,
                                                SDLK_RETURN, SDLK_UP, SDLK_DOWN,
                                                SDLK_LEFT, SDLK_RIGHT };

    // The name of the default configuration file.
    const char *kDefaultConfig_ = "ndb.cfg";

    // Holds the current button mapping for the controller.
    SDL_Keycode button_map_[NUM_BUTTONS];

    // Holds the current pressed/released state for each button.
    DataWord input_status_ = 0;

    // Used to prevent the play from pressing opposing dpad directions
    // at the same time.
    bool dpad_priority_up_ = false;
    bool dpad_priority_left_ = false;

    // Helper functions.
    void CreateConfig(FILE *config);
    void LoadConfig(FILE *config);
    bool FileReadChunk(char *buffer, FILE *file, int max_size, char term);
    void SetMap(char *cfg, char *key);

  public:
    // Loads the given config file, or a default if none is specified.
    Input(char *file);

    // Presses the given key, if it's mapped.
    void Press(SDL_Keycode key);

    // Releases the given key, if it's mapped.
    void Release(SDL_Keycode key);

    // Returns a byte containing the current valid button presses.
    DataWord Poll(void);
};

#endif
