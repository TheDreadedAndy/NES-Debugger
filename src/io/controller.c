/*
 * Translates the controller byte from input.c into the format expected
 * to be produced by an NES. Currently only supports a standard controller.
 */

#include <stdlib.h>
#include "../sdl/input.h"
#include "../util/data.h"

// Each controller is mapped to a specific address.
#define JOY1_ADDR 0x4016U
#define JOY2_ADDR 0x4017U

// Each controller has a shift register for its inputs.
word_t joy1_shift = 0xFF;
word_t joy2_shift = 0xFF;

// Additionally, standard controllers are polled for input
// whenever the strobe bit is set.
word_t joy_strobe = 0;

/*
 * Reads from the specified controller port, shifting it.
 */
word_t controller_read(dword_t addr) {
  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe) {
    joy1_shift = input_poll();
    joy2_shift = 0x00U; // No P2 Emulation.
  }

  // Get the button being pressed for the requested controller and shift
  // its register to the next button.
  word_t press = 0xFFU;
  if (addr == JOY1_ADDR) {
    press = (joy1_shift & 1);
    joy1_shift = 0x80U | (joy1_shift >> 1);
  } else if (addr == JOY2_ADDR) {
    press = (joy2_shift & 2);
    joy2_shift = 0x80U | (joy2_shift >> 1);
  }

  // Return the requested button press.
  return press;
}

/*
 * Writes to the controller registers, updating the strobe
 * and shift registers as needed.
 */
void controller_write(word_t val, dword_t addr) {
  // Update the strobe register if its address was written to.
  if (addr == JOY1_ADDR) { joy_strobe = (val & 1); }

  // Poll for SDL inputs, if strobe is active.
  if (joy_strobe) {
    joy1_shift = input_poll();
    joy2_shift = 0x00U; // No P2 Emulation.
  }

  return;
}
