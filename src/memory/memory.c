/*
 * NES memory implementation.
 * Abstracts away mappers and memory mapping from the 2A03.
 * Loading the rom and dealing with it's mapper are handled
 * in this file. The NES2/INES header are parsed in memory_new().
 * Currently only supports mapper 2.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./uxrom.h"

// Creates a new memory structure.
// Currently only supports mapper 2.
memory_t *memory_new(char *file) {
  // Read in INES/NES2.0 header.
  FILE *rom = fopen(file, "r");
  char *header = xmalloc(HEADER_SIZE * sizeof(uint8_t));
  for (size_t i = 0; i < HEADER_SIZE; i++) {
    header[i] = (uint8_t)fgetc(rom);
  }

  //TODO: change this to case on the mapper number.
  memory_t *M = uxrom_new(header, rom);

  // Cleanup and exit.
  fclose(rom);
  return M;
}

// Reads memory from a memory structure.
uint8_t memory_read(uint8_t locL, uint8_t locH, memory_t *M) {
  return M->read(locL, locH, M->map);
}

// Writes memory to the memory structure.
void memory_write(uint8_t val, uint8_t locL, uint8_t locH, memory_t *M) {
  M->write(val, locL, locH, M->map);
  return;
}

// Frees a memory structure.
void memory_free(memory_t *M) {
  M->free(M->map);
  free(M->header);
  free(M);
}
