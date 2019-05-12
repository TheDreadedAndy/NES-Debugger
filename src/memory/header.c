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

// The number of bytes each increment in an INES PRG-ROM field represents.
#define INES_PRG_ROM_CHUNKSIZE (1 << 14)

// The number of bytes each increment in an INES CHR-ROM field represents.
#define INES_CHR_ROM_CHUNKSIZE (1 << 13)

/* Global constants */

// The first four bytes of all NES files should be this string.
static const char *ines_preface = "NES\26";

/* Function definitions. */
nes_header_t get_header_type(char *file_header);
void decode_archaic_ines(header_t *header, char *file_header);
void decode_ines(header_t *header, char *file_header);
void decode_nes2(header_t *header, char *file_header);

/*
 * Takes in a 16-byte header and returns the corresponding
 * header structure.
 *
 * Returns a header structure on success and NULL otherwise.
 * Fails if the header is invalid.
 */
header_t *decode_header(char *file_header) {
  // Verify that the header is correct.
  if (strncmp(file_header, ines_preface, PREFACE_SIZE)) {
    fprintf(stderr, "Error: the provided file is not an NES file");
    return NULL;
  }

  // Allocate the header structure and determine the header type.
  header_t *header = xcalloc(1, sizeof(header_t));
  header->header_type = get_header_type(file_header);

  // Decode the header according to its type.
  switch (header->header_type) {
    case ARCHAIC_INES:
      fprintf(stderr, "Warning: Provide rom has an Archaic INES header\n");
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
  // Byte 7, bits 2 and 3, of a nes file header contain an
  // indicator for whether or not the header is in the NES 2.0 format.
  // If the bits are equal to 2, it's a NES 2.0 header.
  if ((file_header[7] & 0x0C) == 0x08) {
    return NES2;
  }

  // Most roms with headers in the Archaic INES format had
  // some kind of text filling their final bytes. In general, if
  // the headers final 4 bytes are 0 and it's not a NES 2.0 header,
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
