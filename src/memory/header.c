#include <stdlib.h>
#include <stdint.h>
#include "./header.h"

/* Defined constants */

#define HEADER_SIZE 4

/* Global constants */

// The first four bytes of all NES files should be this string.
static const char file_header = "NES\26";

/* Function definitions. */
//...

/*
 * Takes in a 16-byte header and returns the corresponding
 * header structure.
 *
 * Returns a header structure on success and NULL otherwise.
 * Fails if the header is invalid.
 */
header_t *decode_header(char *header) {
  // Verify that the header is correct.
  if (strncmp(header, file_header, HEADER_SIZE)) {
    fprintf(stderr, "Error: the provided file is not an NES file");
    exit(1);
  }

  // Allocate the header structure and determine the header type.
  header_t *decoded_header = xcalloc(1, sizeof(header_t));
  decoded_header->header_type = get_header_type(header);

  // Decode the header according to its type.
  switch (decoded_header->header_type) {
    case ARCHAIC_INES:
      fprintf(stderr, "Warning: Provide rom has an Archaic INES header\n");
      decode_archaic_ines(decoded_header, header);
      break;
    case INES:
      decode_ines(decoded_header, header);
      break;
    case NES2:
      decode_nes2(decoded_header, header);
      break;
  }

  return decoded_header;
}
