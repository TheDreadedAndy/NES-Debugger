/*
 * All correctly formated NES roms are preceded by a 16-byte header,
 * which encodes information about the chips contained on the board of the
 * original rom.
 *
 * The purpose of this file is to decode that information
 * into an easy to use structure, which can then be accessed by any
 * mapper implementations. Doing this prevents every mapper implementation
 * from having to include its own logic to decode the header.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "./header.h"
#include "../util/util.h"
#include "../util/data.h"
#include "../util/contracts.h"

/* Defined constants */

// Size of the "preface" to any NES header, in bytes. The preface should
// always be NES followed by a DOS EOF.
#define PREFACE_SIZE 4

// Bytes 4 and 5 of all header types denote the prg-rom and chr-rom size,
// respectively.
#define PRG_ROM_SIZE_LSB 4
#define CHR_ROM_SIZE_LSB 5

// Every header has byte 6 mapped to a set of 4 flags and the lower nyble
// of the mapper number. This is the last byte defined in the archaic standard.
#define FLAG_6 6

// Byte 7 contains the console type and the high nyble of the mapper number.
#define FLAG_7 7

// Bytes 8 and 9 hold the PRG-RAM size and tv system bool in the INES
// standard, respectively. These are the last bytes in the standard.
#define INES_PRG_RAM_SIZE 8
#define INES_TV_SYSTEM 9

// The number of bytes each increment in the PRG-ROM field represents.
#define PRG_ROM_CHUNKSIZE 0x4000U

// The number of bytes each increment in the CHR-ROM field represents.
#define CHR_ROM_CHUNKSIZE 0x2000U

// The number of bytes each increment in the INES PRG-RAM field represents.
#define INES_PRG_RAM_CHUNKSIZE 0x2000U

// The number of bytes CHR-RAM can be set to hold in the INES format.
#define INES_CHR_RAM_SIZE 0x2000U

/* Global constants */

// The first four bytes of all NES files should be this string.
static const char *kInesPreface = "NES\x1A";

/* Function definitions. */
static NesHeaderType GetHeaderType(char *file_header, size_t rom_size);
static void DecodeArchaicInes(RomHeader *header, char *file_header);
static size_t GetInesPrgRomSize(char *file_header);
static size_t GetInesChrRomSize(char *file_header);
static void DecodeFlag6(RomHeader *header, char *file_header);
static void DecodeInes(RomHeader *header, char *file_header);
static size_t GetInesMapper(char *file_header);
static size_t GetInesPrgRamSize(char *file_header);
static void DecodeInesBools(RomHeader *header, char *file_header);
static void DecodeNes2(RomHeader *header, char *file_header);
static size_t GetNes2PrgRomSize(char *file_header);
static size_t GetNes2ChrRomSize(char *file_header);
static size_t GetNes2RomSectionSize(DataWord lsb, DataWord msb,
                                    size_t unit_size);

/*
 * Takes in a rom file and returns the corresponding header structure.
 *
 * Assumes the rom file is open and valid.
 *
 * Returns a header structure on success and NULL otherwise.
 * Fails if the header is invalid.
 */
RomHeader *DecodeHeader(FILE *rom_file) {
  // Read the header in from the file.
  fseek(rom_file, 0, SEEK_SET);
  char *file_header = new char[HEADER_SIZE];
  for (size_t i = 0; i < HEADER_SIZE; i++) { file_header[i] = fgetc(rom_file); }

  // Calculate the size of the rom file.
  size_t rom_size = GetFileSize(rom_file);

  // Verify that the header is correct.
  if (strncmp(file_header, kInesPreface, PREFACE_SIZE)) {
    fprintf(stderr, "Error: the provided file is not an NES file\n");
    return NULL;
  }

  // Allocate the header structure and determine the header type.
  RomHeader *header = new RomHeader;
  header->header_type = GetHeaderType(file_header, rom_size);

  // Decode the header according to its type.
  switch (header->header_type) {
    case ARCHAIC_INES:
      fprintf(stderr, "Warning: Provided rom has an Archaic INES header\n");
      DecodeArchaicInes(header, file_header);
      break;
    case INES:
      DecodeInes(header, file_header);
      break;
    case NES2:
      DecodeNes2(header, file_header);
      break;
  }

  // Clean up and exit
  delete[] file_header;
  CONTRACT((header->prg_rom_size + header->chr_rom_size + 16) == rom_size);
  return header;
}

/*
 * Determines the format of the file header of a rom,
 * using a strategy outlined on nesdev.
 *
 * Assumes the header is non-null.
 */
static NesHeaderType GetHeaderType(char *file_header, size_t rom_size) {
  /*
   * Byte 7, bits 2 and 3, of a nes file header contain an
   * indicator for whether or not the header is in the NES 2.0 format.
   * If the bits are equal to 2, it's a NES 2.0 header.
   * The problem with this is that an archaic INES header may set these bits,
   * so we must confirm the file size is correct to ensure it is NES2.
   */

  // Get the size of the rom as if it was a nes2 header.
  size_t size = GetNes2PrgRomSize(file_header)
              + GetNes2ChrRomSize(file_header) + HEADER_SIZE;

  // Determine if it's a nes2 header.
  if (((file_header[7] & 0x0C) == 0x08) && size <= rom_size) {
    return NES2;
  }

  // Most roms with headers in the Archaic INES format had
  // some kind of text filling their final bytes. In general, if
  // the headers final 4 bytes are 0, and it's not a NES 2.0 header,
  // then it's an INES header.
  for (size_t i = HEADER_SIZE - 4; i < HEADER_SIZE; i++) {
    if (file_header[i] != 0) {
      return ARCHAIC_INES;
    }
  }

  return INES;
}

/*
 * Decodes an archaic INES header into the header structure.
 *
 * Assumes that the file header is non-null, 16 bytes long, and in
 * the archaic INES format.
 * Assumes the header structure is non-null.
 */
static void DecodeArchaicInes(RomHeader *header, char *file_header) {
  // Gets the ram sizes using the INES standard.
  header->prg_rom_size = GetInesPrgRomSize(file_header);
  header->chr_rom_size = GetInesChrRomSize(file_header);

  // When an INES header does not specify a size for CHR-ROM, 8K of CHR-RAM
  // is assumed to be present.
  if (header->chr_rom_size == 0) {
    header->chr_ram_size = INES_CHR_RAM_SIZE;
  }

  // The archaic ines format supported only 16 mappers, which were
  // determined by the upper four bits of flag 6.
  header->mapper = ((static_cast<DataWord>(file_header[FLAG_6])) >> 4);

  // All three formats use the same flags in byte 6.
  DecodeFlag6(header, file_header);

  return;
}

/*
 * Calculates the size of the PRG-ROM using the given header.
 *
 * Assumes the header is non-null, 16 bytes long, and in an INES format.
 */
static size_t GetInesPrgRomSize(char *file_header) {
  return (static_cast<size_t>(file_header[PRG_ROM_SIZE_LSB]))
                            * PRG_ROM_CHUNKSIZE;
}

/*
 * Calculates the size of the CHR-ROM using the given header.
 *
 * Assumes the header is non-null, 16 bytes long, and in an INES format.
 */
static size_t GetInesChrRomSize(char *file_header) {
  return (static_cast<size_t>(file_header[CHR_ROM_SIZE_LSB]))
                            * CHR_ROM_CHUNKSIZE;
}

/*
 * Decodes byte 6 of an NES header into the mirroring, battery, trainer, and
 * four screen bools.
 *
 * Assumes the header is non-null and 16 bytes long.
 */
static void DecodeFlag6(RomHeader *header, char *file_header) {
  header->mirror = static_cast<bool>(file_header[FLAG_6] & 0x01);
  header->battery = static_cast<bool>(file_header[FLAG_6] & 0x02);
  header->trainer = static_cast<bool>(file_header[FLAG_6] & 0x04);
  header->four_screen = static_cast<bool>(file_header[FLAG_6] & 0x08);
  return;
}

/*
 * Decodes an INES header into the header structure.
 *
 * Assumes the file header is non-null, 16 bytes long, and in the INES format.
 * Assumes the header structure is non-null.
 */
static void DecodeInes(RomHeader *header, char *file_header) {
  // Get the ram sizes using the INES standard.
  header->prg_rom_size = GetInesPrgRomSize(file_header);
  header->chr_rom_size = GetInesChrRomSize(file_header);

  // When an INES header does not specify a size for CHR-ROM, CHR-RAM
  // is assumed to be present. This ram size is not specified, but here we
  // assume it to be 8KB.
  if (header->chr_rom_size == 0) {
    header->chr_ram_size = INES_CHR_RAM_SIZE;
  }

  // Get the header using the INES standard.
  header->mapper = GetInesMapper(file_header);

  // Decode the universal flag byte.
  DecodeFlag6(header, file_header);

  // Get the ram size.
  header->prg_ram_size = GetInesPrgRamSize(file_header);

  // Decode the remaining flags and exit.
  DecodeInesBools(header, file_header);
  return;
}

/*
 * Determines the mapper number specified by an INES header.
 *
 * Assumes the file header is non-null, 16 bytes long, and in the INES format.
 */
static size_t GetInesMapper(char *file_header) {
  return (file_header[FLAG_7] & 0xf0U)
       | ((static_cast<DataWord>(file_header[FLAG_6])) >> 4);
}

/*
 * Determines the PRG-RAM size from an INES header.
 *
 * Assumes the file header is non-null, 16 bytes long, and in the INES format.
 */
static size_t GetInesPrgRamSize(char *file_header) {
  if ((static_cast<size_t>(file_header[INES_PRG_RAM_SIZE])) == 0) {
    // For compatibility reasons, 8KB of ram is always given to the rom, even
    // in mappers where it did not actually exist (like UxROM).
    return INES_PRG_RAM_CHUNKSIZE;
  } else {
    return (static_cast<size_t>(file_header[INES_PRG_RAM_SIZE]))
                              * INES_PRG_RAM_CHUNKSIZE;
  }
}

/*
 * Gets the VS and TV system bools from an INES header.
 *
 * Assumes the file header is non-null, 16 bytes long, and in the INES format.
 */
static void DecodeInesBools(RomHeader *header, char *file_header) {
  // Convert the VS bool to the header enum.
  if (file_header[FLAG_7] & 0x01) {
    header->console_type = VS;
  } else {
    header->console_type = NES;
  }

  // Convert the TV system bool to the header enum.
  if (file_header[INES_TV_SYSTEM] & 0x01) {
    header->tv_type = PAL_TV;
  } else {
    header->tv_type = NTSC_TV;
  }

  return;
}

/*
 * Decodes an NES 2.0 header into the header structure.
 *
 * Assumes the file header is non-null, 16 bytes long, and in the
 * NES 2.0 format.
 * Assumes the structure is non-null.
 */
static void DecodeNes2(RomHeader *header, char *file_header) {
  // TODO: Implement NES 2.0 header decoding.
  fprintf(stderr,
          "Warning: NES 2.0 headers are not implemented. Decoding as INES\n");
  header->header_type = INES;
  DecodeInes(header, file_header);
  return;
}

/*
 * Takes in a 16 byte header and calculates the size of the program rom.
 *
 * Assumes the header is non-null, 16 bytes long, and in the NES 2.0 format.
 */
static size_t GetNes2PrgRomSize(char *file_header) {
  DataWord prg_lsb = static_cast<DataWord>(file_header[PRG_ROM_SIZE_LSB]);
  DataWord prg_msb = static_cast<DataWord>(file_header[9] & 0x0fU);
  return GetNes2RomSectionSize(prg_lsb, prg_msb, PRG_ROM_CHUNKSIZE);
}

/*
 * Takes in a 16 byte header and calculates the size of the chr rom.
 *
 * Assumes the header is non-null, 16 bytes long, and in the NES 2.0 format.
 */
static size_t GetNes2ChrRomSize(char *file_header) {
  DataWord chr_lsb = static_cast<DataWord>(file_header[CHR_ROM_SIZE_LSB]);
  DataWord chr_msb = (static_cast<DataWord>(file_header[9])) >> 4;
  return GetNes2RomSectionSize(chr_lsb, chr_msb, CHR_ROM_CHUNKSIZE);
}

/*
 * Calculates the size of a rom section using the standard nes2 formula.
 *
 * Assumes the unit_size is correct for the section being calculated, and
 * that the high 4 bits of the msb are 0.
 */
static size_t GetNes2RomSectionSize(DataWord lsb, DataWord msb,
                                    size_t unit_size) {
  if (msb == 0xfU) {
    // If the msb is 0xf, then the size follows the formula 2^E * (MM * 2 + 1),
    // where E is the high 6 bits of the lsb and MM is the lower 2 bits.
    return (1 << (lsb & 0xFC)) * (((lsb & 0x03) << 1) + 1);
  } else {
    // Otherwise, the msb and lsb are considered as one number and multiplied
    // by the given unit size.
    return (((static_cast<size_t>(msb)) << 8) | (static_cast<size_t>(lsb)))
                                              * unit_size;
  }
}
