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
#include "../memory/memory.h"

// The number of different NES colors in a given palette.
#define PALETTE_SIZE 0x40U

// The number of NES palettes.
#define PALETTE_DIMS 8U

// The files size of an NES palette.
#define PALETTE_FILE_SIZE 1536U

// Used to access PPUMASK and update the settings of the palette.
#define FLAG_GRAYSCALE 0x01U
#define FLAG_COLOR_TINT 0xE0U
#define COLOR_TINT_SHIFT 5U

// Used to force a given color to grayscale.
#define GRAYSCALE_MASK 0x30U

// Holds the decoded palette, which is used by palette_decode.
uint32_t *decoded_palette = NULL;

// Holds the index for the palette being accessed. Controlled by the emphasis
// bits in PPUMASK.
word_t color_tint = 0;

// Determines if a greyscale effect should be applied to requested colors.
// Controlled by PPUMASK.
bool grayscale_colors = false;

/* Helper Functions */
bool palette_invalid(FILE *pal_file);

/*
 * Initializes the palette using the given file.
 *
 * Assumes the specified file is non-NULL.
 */
void palette_init(char *file) {
  // This variable contains the default palette file.
  extern const word_t _binary_bins_nes_palette_bin_start[];

  // Open the palette file and verify it is in the proper format.
  FILE *pal_file = NULL;
  if (file != NULL) {
    pal_file = fopen(file, "rb");
    if ((pal_file == NULL) || palette_invalid(pal_file)) {
      fprintf(stderr, "Invalid palette file.\n");
    }
  }

  // Load the palette file into the decoded palette array.
  decoded_palette = xcalloc(PALETTE_SIZE * PALETTE_DIMS, sizeof(uint32_t));
  uint32_t red, green, blue;
  if (pal_file != NULL) {
    // Load the file the user provided.
    fseek(pal_file, 0, SEEK_SET);
    for (size_t i = 0; i < (PALETTE_SIZE * PALETTE_DIMS); i++) {
      red = (word_t) fgetc(pal_file);
      green = (word_t) fgetc(pal_file);
      blue = (word_t) fgetc(pal_file);
      decoded_palette[i] = (red << 16) | (green << 8) | blue;
    }
  } else {
    // If the file was invalid (or not provided), load the default instead.
    for (size_t i = 0; i < (PALETTE_SIZE * PALETTE_DIMS); i++) {
      red = _binary_bins_nes_palette_bin_start[3 * i + 0];
      green = _binary_bins_nes_palette_bin_start[3 * i + 1];
      blue = _binary_bins_nes_palette_bin_start[3 * i + 2];
      decoded_palette[i] = (red << 16) | (green << 8) | blue;
    }
  }

  // Clean up and return success.
  if (pal_file != NULL) { fclose(pal_file); }
  return;
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
  color = (grayscale_colors) ? (color & GRAYSCALE_MASK) : (color & PIXEL_MASK);
  return decoded_palette[(color_tint * PALETTE_SIZE) + color];
}

/*
 * Updates the settings of the palette controlled by PPUMASK.
 *
 * Assumes the palette has been initialized.
 */
void palette_update_mask(word_t mask) {
  // Update the palette settings.
  grayscale_colors = mask & FLAG_GRAYSCALE;
  color_tint = (mask & FLAG_COLOR_TINT) >> COLOR_TINT_SHIFT;

  // Signal memory to update its copies of the decoded palette pixels.
  memory_palette_update();
  return;
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
