#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "./header.h"
#include "../util/util.h"

/* Defined constants */

// Size of the "preface" to any NES header, in bytes. The preface should
// always be NES followed by a DOS EOF.
#define PREFACE_SIZE 4

// Total size of any NES header in bytes.
#define HEADER_SIZE 16

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
#define PRG_ROM_CHUNKSIZE (1 << 14)

// The number of bytes each increment in the CHR-ROM field represents.
#define CHR_ROM_CHUNKSIZE (1 << 13)

/* Global constants */

// The first four bytes of all NES files should be this string.
static const char *ines_preface = "NES\26";

/* Function definitions. */
nes_header_t get_header_type(char *file_header, size_t rom_size);
void decode_archaic_ines(header_t *header, char *file_header);
void decode_ines(header_t *header, char *file_header);
void decode_nes2(header_t *header, char *file_header);
size_t get_nes2_prg_rom_size(char *file_header);
size_t get_nes2_chr_rom_size(char *file_header);
size_t get_nes2_rom_section_size(word_t lsb, word_t msb, size_t unit_size);

/*
 * Takes in a 16-byte header and returns the corresponding
 * header structure.
 *
 * Returns a header structure on success and NULL otherwise.
 * Fails if the header is invalid.
 */
header_t *decode_header(FILE *rom_file, size_t rom_size) {
  // Verify that the header is correct.
  if (strncmp(file_header, ines_preface, PREFACE_SIZE)) {
    fprintf(stderr, "Error: the provided file is not an NES file");
    return NULL;
  }

  // Allocate the header structure and determine the header type.
  header_t *header = xcalloc(1, sizeof(header_t));
  header->header_type = get_header_type(file_header, rom_size);

  // Decode the header according to its type.
  switch (header->header_type) {
    case ARCHAIC_INES:
      fprintf(stderr, "Warning: Provided rom has an Archaic INES header\n");
      decode_archaic_ines(header, file_header);
      break;
    case INES:
      decode_ines(header, file_header);
      break;
    case NES2:
      decode_nes2(header, file_header);
      break;
  }

  return header;
}

/*
 * Determines the format of the file header of a rom, using a
 * strategy similar to the one outlined on nesdev.
 *
 * Assumes the header is non-null.
 */
nes_header_t get_header_type(char *file_header) {
  /*
   * Byte 7, bits 2 and 3, of a nes file header contain an
   * indicator for whether or not the header is in the NES 2.0 format.
   * If the bits are equal to 2, it's a NES 2.0 header.
   * The problem with this is that an archaic INES header may set these bits,
   * so we must confirm the file size is correct to ensure it is NES2.
   */

  // Get the size of the rom as if it was a nes2 header.
  size_t size = get_nes2_prg_rom_size(file_header)
              + get_nes2_chr_rom_size(file_header) + HEADER_SIZE;

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
 * TODO
 */
void decode_archaic_ines(header_t *header, char* file_header) {
  for (size_t i = 0; i < PREFACE_SIZE; i++) {
    header->file_header[i] = file_header[i];
  }
}

/*
 * TODO
 */
void decode_ines(header_t *header, char* file_header) {
  (void)header;
  (void)file_header;
  return;
}

/*
 * TODO
 */
void decode_nes2(header_t *header, char *file_header) {
  (void)header;
  (void)file_header;
  return;
}

/*
 * Takes in a 16 byte header and calculates the size of the program rom.
 *
 * Assumes the header is non-null, 16 bytes long, and in the NES 2.0 format.
 */
size_t get_nes2_prg_rom_size(char *file_header) {
  word_t prg_lsb = (word_t) file_header[PRG_ROM_SIZE_LSB];
  word_t prg_msb = (word_t) (file_header[9] & 0x0fU);
  return get_nes2_rom_section_size(prg_lsb, prg_msb, PRG_ROM_CHUNKSIZE);
}

/*
 * Takes in a 16 byte header and calculates the size of the chr rom.
 *
 * Assumes the header is non-null, 16 bytes long, and in the NES 2.0 format.
 */
size_t get_nes2_chr_rom_size(char *file_header) {
  word_t chr_lsb = (word_t) file_header[CHR_ROM_SIZE_LSB];
  word_t chr_msb = ((word_t) file_header[9]) >> 4;
  return get_nes2_rom_section_size(chr_lsb, prg_msb, CHR_ROM_CHUNKSIZE);
}

/*
 * Calculates the size of a rom section using the standard nes2 formula.
 *
 * Assumes the unit_size is correct for the section being calculated, and
 * that the high 4 bits of the msb are 0.
 */
size_t get_nes2_rom_section_size(word_t lsb, word_t msb, size_t unit_size) {
  if (msb == 0xfU) {
    // If the msb is 0xf, then the size follows the formulat 2^E * (MM * 2 + 1),
    // where E is the high 6 bits of the lsb and MM is the lower 2 bits.
    return (1 << (lsb & 0xFC)) * (((lsb & 0x03) << 1) + 1);
  } else {
    // Otherwise, the msb and lsb are considered as one number and multiplied
    // by the given unit size.
    return ((((size_t) msb) << 8) | ((size_t) lsb)) * unit_size;
  }
}
