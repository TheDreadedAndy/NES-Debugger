#ifndef _NES_CONTROLLER
#define _NES_CONTROLLER

#include <cstdlib>

#include "../util/data.h"
#include "../sdl/input.h"

// The memory mapped addresses controller data can be accessed from.
#define IO_JOY1_ADDR 0x4016U
#define IO_JOY2_ADDR 0x4017U

/*
 * Represents a controller communication between an input object and the
 * memory emulation.
 */
class Controller {
  private:
    // Each controller has a shift register for its inputs.
    DataWord joy1_shift_;
    DataWord joy2_shift_;

    // Standard controllers are probed whenever the strobe bit is set.
    DataWord joy_strobe_;

    // Holds the input object associated with this controller.
    Input *input_;

  public:
    // Creates a controller object.
    Controller(Input *input);

    // Reads from a controller mmio address.
    DataWord Read(DoubleWord addr);

    // Writes to a controller mmio address.
    void Write(DoubleWord addr, DataWord val);

    // Frees the controller object.
    ~Controller(void);
};

#endif
