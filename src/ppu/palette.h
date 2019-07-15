#include "../util/data.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef _NES_PALETTE
#define _NES_PALETTE

// Loads in the palette file to be used for color decoding.
bool palette_init(char *file);

// Decodes an NES color into an ARGB color.
uint32_t palette_decode(word_t color);

// Frees the palette file.
void palette_free(void);

#endif
