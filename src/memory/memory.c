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
#include "./header.h"
#include "./uxrom.h"
#include "../util/data.h"

/*
 * Holds the global memory structure. Unavailable outside this file.
 * Manages mappers, RAM, MMIO, and VRAM
 */
memory_t *system_memory = NULL;

/*
 * Initializes all system memory for the NES emulation.
 *
 * Assumes the system memory has not already been initialized.
 * Assumes that the file is open and valid.
 * Assumes that the provided header is non-null and valid.
 */
bool memory_init(FILE *rom_file, header_t *header) {
  // Use the decoded header to decide which memory structure should be created.
  switch(header->mapper) {
    case UXROM_MAPPER:
      system_memory = uxrom_new(rom_file, header);
      break;
    default:
      fprintf(stderr, "Error: Rom requires unimplemented mapper\n");
      return false;
  }

  // Return the result of the mapper creation as a boolean.
  return system_memory != NULL;
}

/*
 * Uses the generic memory structures read function to read a word
 * from memory.
 *
 * Assumes the memory structure is valid.
 */
word_t memory_read(word_t mem_lo, word_t mem_hi) {
  return system_memory->read(mem_lo, mem_hi, system_memory->map);
}

/*
 * Uses the generic memory structures write function to write a word
 * to memory.
 *
 * Assumes the memory structure is valid.
 */
void memory_write(word_t val, word_t mem_lo, word_t mem_hi) {
  system_memory->write(val, mem_lo, mem_hi, system_memory->map);
  return;
}

/*
 * Frees a generic memory structure.
 * Assumes that the structure is valid.
 */
void memory_free() {
  system_memory->free(system_memory->map);
  free(system_memory->header);
  free(system_memory);
}
