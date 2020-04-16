#ifndef _NES_INES
#define _NES_INES

#include <cstdio>

// Total size of any NES header in bytes.
#define HEADER_SIZE 16U

// Encodes the type of header which the structure was created from.
typedef enum {INES, ARCHAIC_INES, NES2} NesHeaderType;

// The mapper which should be used by the memory system.
typedef enum {NROM = 0, SXROM = 1, UXROM = 2} NesMapperType;

// Encodes the console we need to emulate.
typedef enum {NES, VS, PC10, EXT} NesConsoleType;

// Encodes the tv standard the rom is expecting.
typedef enum {NTSC_TV, PAL_TV} NesTvType;

// Encodes the expected cpu/ppu timing mode.
typedef enum {NTSC, PAL, MULTI, DENDY} NesTimingType;

// TODO
typedef enum {NO_EXT} NesExtension;
typedef enum {NO_VSPPU} NesVsPpuType;
typedef enum {NO_VSHW} NesVsHardware;
typedef enum {NO_EXP} NesExpansion;

/*
 * Contains the decoded archaic INES/INES/NES 2.0 header.
 * Used to determine what should be emulated and how it should be done.
 */
typedef struct {
  // Specifies the type of header that was decoded.
  NesHeaderType header_type;

  // Main memory sizes.
  size_t prg_rom_size;
  size_t prg_ram_size;
  size_t prg_nvram_size;

  // PPU memory sizes.
  size_t chr_rom_size;
  size_t chr_ram_size;
  size_t chr_nvram_size;

  // The id of the memory mapper expected by the cart.
  NesMapperType mapper;
  size_t submapper;

  // Cart information.
  bool mirror;
  bool battery;
  bool trainer;
  bool four_screen;

  // Hardware information
  NesConsoleType console_type;
  NesTvType tv_type;
  NesTimingType timing_mode;
  NesExtension ext_type;
  NesVsPpuType ppu_type;
  NesVsHardware hw_type;

  // The default controller device expected by the cart.
  NesExpansion default_expansion;

  // Number of special chips present in the cart.
  size_t num_misc_roms;
} RomHeader;

// Decodes a 16-byte header into a header structure.
RomHeader *DecodeHeader(FILE *rom_file);

#endif
