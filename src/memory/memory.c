/*
 * NES memory implementation.
 *
 * Abstracts away memory mapping from the 2A03.
 *
 * Loading the rom and dealing with it's mapper is handled using
 * the generic memory structure. Memory_new creates a memory
 * structure which contains a pointer to the proper memory
 * structure for the mapper and the functions to interact with
 * said mapper. The structure also contains the parsed file header.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./uxrom.h"
#include "../util/data.h"

/*
 * Creates a new generic memory structure.
 * In doing so, reads the provided rom file into memory.
 *
 * Assumes that the provided string is non-null and contains
 * the location of a valid NES rom file.
 *
 * Returns a memory structure on success and NULL on failure.
 */
memory_t *memory_new(char *file) {
  // Read in INES/NES2.0 header.
  FILE *rom = fopen(file, "r");
  char *header = xmalloc(HEADER_SIZE * sizeof(word_t));
  for (size_t i = 0; i < HEADER_SIZE; i++) {
    header[i] = (word_t)fgetc(rom);
  }

  //TODO: change this to case on the mapper number.
  memory_t *M = uxrom_new(header, rom);

  // Cleanup and exit.
  fclose(rom);
  return M;
}

/*
 * Uses the generic memory structures read function to read a word
 * from memory.
 *
 * Assumes the memory structure is valid.
 */
word_t memory_read(word_t mem_lo, word_t mem_hi, memory_t *M) {
  return M->read(mem_lo, mem_hi, M->map);
}

/*
 * Uses the generic memory structures write function to write a word
 * to memory.
 *
 * Assumes the memory structure is valid.
 */
void memory_write(word_t val, word_t mem_lo, word_t mem_hi, memory_t *M) {
  M->write(val, mem_lo, mem_hi, M->map);
  return;
}

/*
 * Frees a generic memory structure.
 * Assumes that the structure is valid.
 */
void memory_free(memory_t *M) {
  M->free(M->map);
  free(M->header);
  free(M);
}
