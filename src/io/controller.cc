/*
 * Translates the controller byte from input.c into the format expected
 * to be produced by an NES. Currently only supports a standard controller.
 */

#include "./controller.h"

#include <cstdlib>

#include "../sdl/input.h"
#include "../util/data.h"

/*
 * Creates a controller object.
 */
Controller::Controller(Input *input) {
  // Initializes the shift registers for the controllers.
  joy1_shift_ = 0xFF;
  joy2_shift_ = 0xFF;

  // Stores the given input object.
  input_ = input;

  return;
}

/*
 * Reads from the specified controller port, shifting it.
 */
DataWord Controller::Read(DoubleWord addr) {
  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe_) {
    joy1_shift_ = input_->Poll();
    joy2_shift_ = 0x00U; // No P2 Emulation.
  }

  // Get the button being pressed for the requested controller and shift
  // its register to the next button.
  DataWord press = 0xFFU;
  if (addr == IO_JOY1_ADDR) {
    press = (joy1_shift_ & 1);
    joy1_shift_ = 0x80U | (joy1_shift_ >> 1);
  } else if (addr == IO_JOY2_ADDR) {
    press = (joy2_shift_ & 1);
    joy2_shift_ = 0x80U | (joy2_shift_ >> 1);
  }

  // Return the requested button press.
  return press;
}

/*
 * Writes to the controller registers, updating the strobe
 * and shift registers as needed.
 */
void Controller::Write(DoubleWord addr, DataWord val) {
  // Update the strobe register if its address was written to.
  if (addr == IO_JOY1_ADDR) { joy_strobe_ = (val & 1); }

  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe_) {
    joy1_shift_ = input_->Poll();
    joy2_shift_ = 0x00U; // No P2 Emulation.
  }

  return;
}

/*
 * Deletes the controller object.
 */
Controller::~Controller(void) {
  return;
}
