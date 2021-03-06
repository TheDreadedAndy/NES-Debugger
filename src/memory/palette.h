#ifndef _NES_PALETTE
#define _NES_PALETTE

#include <cstdint>
#include <cstdio>

#include "../util/data.h"

// Palette format constants. Colors are stored as RGB 32-bit.
#define ACTIVE_PALETTE_SIZE 0x20U
#define PALETTE_DEPTH 32
#define PALETTE_RMASK 0x00FF0000U
#define PALETTE_GMASK 0x0000FF00U
#define PALETTE_BMASK 0x000000FFU
#define PIXEL_MASK 0x3FU

// Abstract the format of pixels away from the emulation.
typedef uint32_t Pixel;

/*
 * Contains both the current implementation of pixels and
 * the associated NES pixels. Used internally by rendering to
 * manage pixels and sent to rendering to enable different
 * implementations.
 */
struct PixelPalette {
  DataWord nes[ACTIVE_PALETTE_SIZE];
  Pixel emu[ACTIVE_PALETTE_SIZE];
};

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
    Pixel *decoded_palette_;

  public:
    // Loads in the given palette file for use in decoding colors.
    // If the file is NULL or invalid, a default is used.
    NesPalette(const char *file);

    // Decodes an NES color into an RGB color.
    Pixel Decode(DataWord color);

    // Updates the mask settings of the palette.
    void UpdateMask(DataWord mask);

    // Frees the decoded palette.
    ~NesPalette(void);
};

#endif
