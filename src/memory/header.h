#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef _NES_INES
#define _NES_INES

// Total size of any NES header in bytes.
#define HEADER_SIZE 16U

// Encodes the type of header which the structure was created from.
typedef enum {INES, ARCHAIC_INES, NES2} nes_header_t;

// Encodes the console we need to emulate.
typedef enum {NES, VS, PC10, EXT} console_t;

// Encodes the tv standard the rom is expecting.
typedef enum {NTSC_TV, PAL_TV} tvsystem_t;

// Encodes the expected cpu/ppu timing mode.
typedef enum timing {NTSC, PAL, MULTI, DENDY} timing_t;

// TODO
typedef enum extension {NO_EXT} ext_t;
typedef enum vsppu {NO_VSPPU} vsppu_t;
typedef enum vshw {NO_VSHW} vshw_t;
typedef enum expandion {NO_EXP} expansion_t;

/*
 * Contains the decoded archaic INES/INES/NES 2.0 header.
 * Used to determine what should be emulated and how it should be done.
 */
typedef struct ines_header {
  // Specifies the type of header that was decoded.
  nes_header_t header_type;

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
header_t *decode_header(FILE *rom_file);

#endif
