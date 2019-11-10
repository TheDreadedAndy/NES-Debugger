/*
 * The NesPalette class provides a way for the PPU/Memory to easily
 * convert NES pixels to RGB colors. Each PPU instance has its own
 * associated NesPalette, and each NesPalette has 8 color palettes.
 * These 8 palettes represent the different color tint modes supported
 * by the NES. The PPU emulation can change the palette in use
 * through calls to UpdateMask().
 *
 * Additionally, the NesPalette is used by memory to decode NES pixels
 * into RGB colors that can be passed to the renderer by the PPU. The system
 * is as follows: The PPU writes an NES pixel to memory, memory takes the pixel
 * and uses the palette associated with its PPU to convert the pixel to RGB.
 * This RGB pixel can then be read back by the PPU during rendering, when it
 * is drawing pixels to the screen. The PPU then sends the pixel to the
 * renderer, which displays it.
 *
 * Note that the user can load in a custom NES palette. However, if the palette
 * is invalid, or none is provided, a default will be used.
 */

#include "./palette.h"

#include <new>
#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include "../util/data.h"
#include "../util/util.h"

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

/*
 * Initializes the palette using the given file.
 */
NesPalette::NesPalette(char *file) {
  // This variable contains the default palette file.
  extern const DataWord _binary_bins_nes_palette_bin_start[];

  // Open the palette file and verify it is in the proper format.
  FILE *pal_file = NULL;
  if (file != NULL) {
    pal_file = fopen(file, "rb");
    if (pal_file == NULL) {
      fprintf(stderr, "WARNING: Could not open the specified palette file\n");
    } else if (Invalid(pal_file)) {
      fprintf(stderr, "WARNING: The specified palette file was invalid.\n");
      fclose(pal_file);
      pal_file = NULL;
    }
  }

  // Load the palette file into the decoded palette array.
  decoded_palette_ = new uint32_t[PALETTE_SIZE * PALETTE_DIMS]();
  uint32_t red, green, blue;
  if (pal_file != NULL) {
    // Load the file the user provided.
    fseek(pal_file, 0, SEEK_SET);
    for (size_t i = 0; i < (PALETTE_SIZE * PALETTE_DIMS); i++) {
      red = static_cast<DataWord>(fgetc(pal_file));
      green = static_cast<DataWord>(fgetc(pal_file));
      blue = static_cast<DataWord>(fgetc(pal_file));
      decoded_palette_[i] = (red << 16) | (green << 8) | blue;
    }
  } else {
    // If the file was invalid (or not provided), load the default instead.
    for (size_t i = 0; i < (PALETTE_SIZE * PALETTE_DIMS); i++) {
      red = _binary_bins_nes_palette_bin_start[3 * i + 0];
      green = _binary_bins_nes_palette_bin_start[3 * i + 1];
      blue = _binary_bins_nes_palette_bin_start[3 * i + 2];
      decoded_palette_[i] = (red << 16) | (green << 8) | blue;
    }
  }

  // Clean up and return success.
  if (pal_file != NULL) { fclose(pal_file); }
  return;
}

/*
 * Checks if the given file is in the 8-bit rgb palette format
 * for NES colors (exactly 1536 bytes long).
 */
bool NesPalette::Invalid(FILE *pal_file) {
  return GetFileSize(pal_file) != PALETTE_FILE_SIZE;
}

/*
 * Decodes an NES color into an RGB color using the current color
 * tint settings.
 */
uint32_t NesPalette::Decode(DataWord color) {
  color = (grayscale_colors_) ? (color & GRAYSCALE_MASK) : (color & PIXEL_MASK);
  return decoded_palette_[(color_tint_ * PALETTE_SIZE) + color];
}

/*
 * Updates the settings of the palette color tint using the given
 * value of PPUMASK.
 */
void NesPalette::UpdateMask(DataWord mask) {
  // Update the palette settings.
  grayscale_colors_ = mask & FLAG_GRAYSCALE;
  color_tint_ = (mask & FLAG_COLOR_TINT) >> COLOR_TINT_SHIFT;
  return;
}

/*
 * Frees the decoded palette.
 */
NesPalette::~NesPalette(void) {
  delete[] decoded_palette_;
  return;
}
