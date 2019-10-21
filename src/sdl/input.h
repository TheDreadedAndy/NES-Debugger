#include <stdlib.h>
#include <SDL2/SDL.h>
#include "../util/data.h"

#ifndef _NES_INPUT
#define _NES_INPUT

// The number of buttons on the NES controller.
#define NUM_BUTTONS 8

/*
 * Holds a configuration file and button mapping, which are used to
 * translate key presses into button presses. These button presses
 * can then be polled and used by the emulator.
 */
class Input {
  private:
    // Holds the current button mapping for the controller.
    SDL_Keycode button_map_[NUM_BUTTONS];

    // Holds the current pressed/released state for each button.
    DataWord input_status_;

    // Used to prevent the play from pressing opposing dpad directions
    // at the same time.
    bool dpad_priority_up_;
    bool dpad_priority_left_;

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
