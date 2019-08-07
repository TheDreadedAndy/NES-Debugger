/*
 * This file contains functions which interract with palette data.
 * It is used by the rendering system to convert colors given
 * by the PPU to ARGB colors, which can be used for SDL.
 *
 * At the start of the emulation, the palette must be initialized.
 * During the emulation, colors are decoded.
 * At the end of the emulation, the palette must be freed.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../util/data.h"
#include "../util/util.h"
#include "./palette.h"

// Holds the decoded palette, which is used by palette_decode.
uint32_t *decoded_palette = NULL;

// Mask used to remove bits not used in NES colors.
#define COLOR_MASK 0x3FU

// The number of different NES colors in a given palette.
#define PALETTE_SIZE 64U

// The files size of an NES palette.
#define PALETTE_FILE_SIZE 192U

/* Helper Functions */
bool palette_invalid(FILE *pal_file);

/*
 * Initializes the palette using the given file.
 *
 * Assumes the specified file is non-NULL.
 */
bool palette_init(char *file) {
  // Open the palette file and verify it is in the proper format.
  FILE *pal_file = fopen(file, "rb");
  if (palette_invalid(pal_file)) {
    fprintf(stderr, "Invalid palette file.\n");
    fclose(pal_file);
    return false;
  }

  // Load the palette file into the decoded palette array.
  fseek(pal_file, 0, SEEK_SET);
  decoded_palette = xcalloc(PALETTE_SIZE, sizeof(uint32_t));
  uint32_t red, green, blue;
  for (size_t i = 0; i < PALETTE_SIZE; i++) {
    red = (word_t) fgetc(pal_file);
    green = (word_t) fgetc(pal_file);
    blue = (word_t) fgetc(pal_file);
    decoded_palette[i] = (red << 16) | (green << 8) | blue;
  }

  // Clean up and return success.
  fclose(pal_file);
  return true;
}

/*
 * Checks if the given file is in the 8-bit rgb palette format
 * for NES colors (exactly 192 bytes long).
 */
bool palette_invalid(FILE *pal_file) {
  return get_file_size(pal_file) != PALETTE_FILE_SIZE;
}

/*
 * Decodes an NES color into an ARGB color.
 *
 * Assumes the palette has been initialized.
 */
uint32_t palette_decode(word_t color) {
  return decoded_palette[color & COLOR_MASK];
}

/*
 * Frees the decoded palette.
 *
 * Assumes the palette has been initialized.
 */
void palette_free(void) {
  free(decoded_palette);
  return;
}
