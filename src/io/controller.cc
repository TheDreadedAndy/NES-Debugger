/*
 * Translates the controller byte from input.c into the format expected
 * to be produced by an NES. Currently only supports a standard controller.
 */

#include "./controller.h"

#include <cstdlib>

#include "../sdl/input.h"
#include "../util/data.h"

// Each controller has a shift register for its inputs.
DataWord joy1_shift = 0xFF;
DataWord joy2_shift = 0xFF;

// Additionally, standard controllers are polled for input
// whenever the strobe bit is set.
DataWord joy_strobe = 0;

/*
 * Reads from the specified controller port, shifting it.
 */
DataWord ControllerRead(DoubleWord addr) {
  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe) {
    joy1_shift = InputPoll();
    joy2_shift = 0x00U; // No P2 Emulation.
  }

  // Get the button being pressed for the requested controller and shift
  // its register to the next button.
  DataWord press = 0xFFU;
  if (addr == IO_JOY1_ADDR) {
    press = (joy1_shift & 1);
    joy1_shift = 0x80U | (joy1_shift >> 1);
  } else if (addr == IO_JOY2_ADDR) {
    press = (joy2_shift & 1);
    joy2_shift = 0x80U | (joy2_shift >> 1);
  }

  // Return the requested button press.
  return press;
}

/*
 * Writes to the controller registers, updating the strobe
 * and shift registers as needed.
 */
void ControllerWrite(DoubleWord addr, DataWord val) {
  // Update the strobe register if its address was written to.
  if (addr == IO_JOY1_ADDR) { joy_strobe = (val & 1); }

  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe) {
    joy1_shift = InputPoll();
    joy2_shift = 0x00U; // No P2 Emulation.
  }

  return;
}
