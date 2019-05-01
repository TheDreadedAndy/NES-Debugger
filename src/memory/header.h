#include <stdlib.h>
#include <stdint.h>

#ifndef _NES_INES
#define _NES_INES

// Encodes the type of header which the structure was created from.
typedef enum {ARCHAIC_INES, INES, NES2} nes_header_t;

// Encodes the console we need to emulate.
typedef enum {NES, VS, PC10, EXT} console_t;

// Encodes the tv standard the rom is expecting.
typedef enum {NTSC, PAL} tvsystem_t;

// Encodes the expected cpu/ppu timing mode.
typedef enum {NTSC, PAL, MULTI, DENDY} timing_t;

// TODO
typedef enum {NONE} ext_t;
typedef enum {NONE} vsppu_t;
typedef enum {NONE} vshw_t;
typedef enum {NONE} expansion_t;

/*
 * Contains the decoded archaic INES/INES/NES 2.0 header.
 * Used to determine what should be emulated and how it should be done.
 */
typedef struct ines_header {
  // Specifies the type of header that was decoded.
  nes_header_t header_type;

  // Should always be "NES" followed by a MS-DOS EOF.
  char file_header[4];

  // Main memory sizes.
  size_t prg_rom_size;
  size_t prg_ram_size;
  size_t prg_nvram_size;

  // PPU memory sizes.
  size_t chr_rom_size;
  size_t chr_ram_size;
  size_t chr_nvram_size;

  // The id of the memory mapper expected by the cart.
  size_t mapper;
  size_t submapper;

  // Cart information.
  bool mirror;
  bool battery;
  bool trainer;
  char *trainer;
  bool four_screen;

  // Hardware information
  console_t console_type;
  tvsystem_t tv_type;
  timing_t timing_mode;
  ext_t ext_type;
  vsppu_t ppu_type;
  vshw_t hw_type;

  // The default controller device expected by the cart.
  expansion_t default_expansion;

  // Number of special chips present in the cart.
  size_t num_misc_roms;
} header_t;

// Decodes a 16-byte header into a header structure.
header_t *decode_header(char *header);

#endif