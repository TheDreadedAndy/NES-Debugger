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

// RAM data constants.
#define RAM_SIZE 0x800U
#define RAM_MASK 0x7FFU

// The address at which PPU registers are located.
#define PPU_OFFSET 0x2000U

// The address at which APU and IO registers are located.
#define IO_OFFSET 0x4000U

// The address at which memory begins  to depend on the mapper in use.
#define MAPPER_OFFSET 0x4020U

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
  // Allocate the generic memory structure.
  system_memory = xcalloc(1, sizeof(memory_t));
  system_memory->ram = xcalloc(sizeof(word_t), RAM_SIZE);
  system_memory->header = header;

  // Use the decoded header to decide which memory structure should be created.
  switch(header->mapper) {
    case UXROM_MAPPER:
      uxrom_new(rom_file, system_memory);
      break;
    default:
      fprintf(stderr, "Error: Rom requires unimplemented mapper: %d\n",
              (unsigned int) header->mapper);
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
  dword_t addr = get_dword(mem_lo, mem_hi);

  // Determine if the NES address space or the mapper should be accessed.
  if (addr < PPU_OFFSET) {
    // Access standard ram.
    return system_memory->ram[addr & RAM_MASK];
  } else if (addr < IO_OFFSET) {
    // Access PPU MMIO.
    return 0; // TODO.
  } else if (addr < MAPPER_OFFSET) {
    // Access APU and IO registers.
    return 0; // TODO.
  } else {
    // Access cartridge space using the mapper.
    return system_memory->read(addr, system_memory->map);
  }
}

/*
 * Uses the generic memory structures write function to write a word
 * to memory.
 *
 * Assumes the memory structure is valid.
 */
void memory_write(word_t val, word_t mem_lo, word_t mem_hi) {
  dword_t addr = get_dword(mem_lo, mem_hi);

  // Determine if the NES address space or the mapper should be accessed.
  if (addr < PPU_OFFSET) {
    // Access standard ram.
    system_memory->ram[addr & RAM_MASK] = val;
  } else if (addr < IO_OFFSET) {
    // Access PPU MMIO.
    // TODO.
  } else if (addr < MAPPER_OFFSET) {
    // Access APU and IO registers.
    // TODO.
  } else {
    system_memory->write(val, addr, system_memory->map);
  }

  return;
}

/*
 * Uses the generic memory structures vram read function to read
 * a word from vram.
 *
 * Assumes memory has been initialized.
 */
word_t memory_vram_read(dword_t addr) {
  return system_memory->vram_read(addr, system_memory->map);
}

/*
 * Uses the generic memory structures vram write function to write
 * a word to vram.
 *
 * Assumes memory has been initialized.
 */
void memory_vram_write(word_t val, dword_t addr) {
  system_memory->vram_write(val, addr, system_memory->map);
  return;
}

/*
 * Frees the generic memory structure.
 * Assumes that the structure is valid.
 */
void memory_free(void) {
  // Free the mapper structure using its specified function.
  system_memory->free(system_memory->map);

  // Free the generic structure.
  free(system_memory->header);
  free(system_memory->ram);
  free(system_memory);
}
