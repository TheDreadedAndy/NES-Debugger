#ifndef _NES_CONTROLLER
#define _NES_CONTROLLER

#include <cstdlib>

#include "../util/data.h"

// The memory mapped addresses controller data can be accessed from.
#define IO_JOY1_ADDR 0x4016U
#define IO_JOY2_ADDR 0x4017U

// Reads from a controller mmio address.
DataWord ControllerRead(DoubleWord addr);

// Writes to a controller mmio address.
void ControllerWrite(DoubleWord addr, DataWord val);

#endif
