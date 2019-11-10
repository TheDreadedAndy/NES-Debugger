#ifndef _NES_PALETTE
#define _NES_PALETTE

#include <cstdint>
#include <cstdio>

#include "../util/data.h"

// Palette format constants. Colors are stored as RGB 32-bit.
#define PALETTE_DEPTH 32
#define PALETTE_RMASK 0x00FF0000U
#define PALETTE_GMASK 0x0000FF00U
#define PALETTE_BMASK 0x000000FFU
#define PIXEL_MASK 0x3FU

/*
 * Uses the provided file as an NES palette, allowing colors to be decoded
 * into RGB colors.
 *
 * Color tints can be selected by providing a PPU mask value through the
 * UpdateMask function.
 */
class NesPalette {
  private:
    // Checks if the loaded palette file was valid.
    bool Invalid(FILE *pal_file);

    // Holds the color emphasis and grayscale value, which reflect
    // the settings in PPUMASK.
    DataWord color_tint_ = 0;
    bool grayscale_colors_ = false;

    // Stores the decoded paletter, where each index holds its
    // corresponding color.
    uint32_t *decoded_palette_;

  public:
    // Loads in the given palette file for use in decoding colors.
    // If the file is NULL or invalid, a default is used.
    NesPalette(char *file);

    // Decodes an NES color into an RGB color.
    uint32_t Decode(DataWord color);

    // Updates the mask settings of the palette.
    void UpdateMask(DataWord mask);

    // Frees the decoded palette.
    ~NesPalette(void);
};

#endif
