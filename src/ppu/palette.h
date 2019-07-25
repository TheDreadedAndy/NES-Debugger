#include "../util/data.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef _NES_PALETTE
#define _NES_PALETTE

// Palette format constants. Colors are stored as ARGB 32-bit.
#define PALETTE_DEPTH 32
#define PALETTE_RMASK 0x00FF0000U
#define PALETTE_GMASK 0x0000FF00U
#define PALETTE_BMASK 0x000000FFU

// Loads in the palette file to be used for color decoding.
bool palette_init(char *file);

// Decodes an NES color into an ARGB color.
uint32_t palette_decode(word_t color);

// Frees the palette file.
void palette_free(void);

#endif
