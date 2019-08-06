#include <stdlib.h>
#include "../util/data.h"

#ifndef _NES_CONTROLLER
#define _NES_CONTROLLER

// Reads from a controller mmio address.
word_t controller_read(dword_t addr);

// Writes to a controller mmio address.
void controller_write(word_t val, dword_t addr);

#endif
