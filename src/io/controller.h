#include <stdlib.h>
#include "../util/data.h"

#ifndef _NES_CONTROLLER
#define _NES_CONTROLLER

// The memory mapped addresses controller data can be accessed from.
#define IO_JOY1_ADDR 0x4016U
#define IO_JOY2_ADDR 0x4017U

// Reads from a controller mmio address.
word_t controller_read(dword_t addr);

// Writes to a controller mmio address.
void controller_write(word_t val, dword_t addr);

#endif
