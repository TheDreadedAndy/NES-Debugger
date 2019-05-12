/*
 * Implementation of INES Mapper 2 (UxROM).
 *
 * The third quarter of addressable NES memory is mapped to a switchable
 * bank, which can be changed by writing to that section of memory.
 * The last quarter of memory is always mapped to the final bank.
 *
 * This implementation can address up to 256KB of cart memory.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./uxrom.h"
#include "../util/data.h"

/*
 * Takes in an INES/NES2 header and creates a uxrom
 * memory system using it. Assumes the header is valid.
 *
 * Returns a generic memory structure, which can be used
 * to interact with the uxrom structure.
 */
memory_t *uxrom_new(char *header, FILE *rom) {
  // Allocate memory structure and set up its data.
  memory_t *M = xcalloc(1, sizeof(memory_t));
  uxrom_t *map = xcalloc(1, sizeof(uxrom_t));
  M->map = (void*) map;
  M->read = &uxrom_read;
  M->write = &uxrom_write;
  M->free = &uxrom_free;
  M->header = header;

  // Set up the memory system itself.
  map->ram = xcalloc(RAM_SIZE, sizeof(word_t));

  // Set up the memory mapped IO.
  // TODO: change these into actual mappings.
  map->ppu = xcalloc(PPU_SIZE, sizeof(word_t));
  map->io = xcalloc(IO_SIZE, sizeof(word_t));
  map->bat = xcalloc(BAT_SIZE, sizeof(word_t));

  // Caclulate rom size and load it into memory.
  size_t num_banks = (size_t)M->header[INES_PRGROM];
  for (size_t i = 0; i < num_banks; i++) {
    map->cart[i] = xmalloc(BANK_SIZE * sizeof(word_t));
    for (size_t j = 0; j < BANK_SIZE; j++) {
      map->cart[i][j] = fgetc(rom);
    }
  }
  map->current_bank = 0;
  map->fixed_bank = num_banks - 1;

  return M;
}

/*
 * Takes in two words and a generic mapper pointer.
 * Uses these bytes to address the memory in the mapper.
 *
 * Assumes that the pointer is non-null and points to a valid
 * UxROM mapper.
 * Assumes that the address formed by the bytes is valid.
 */
word_t uxrom_read(word_t mem_lo, word_t mem_hi, void *map) {
  // Cast back from generic pointer to the memory structure.
  uxrom_t *M = (uxrom_t*) map;

  // Detect where in memory we need to access and do so.
  dword_t addr = get_dword(mem_lo, mem_hi);
  if (addr < 0x2000) {
    return M->ram[addr % RAM_SIZE];
  } else if (addr < 0x4000) {
    return M->ppu[(addr - PPU_OFFSET) % PPU_SIZE];
  } else if (addr < 0x4020) {
    return M->io[addr - IO_OFFSET];
  } else if (addr < 0x6000) {
    printf("FATAL: Memory not implemented");
    abort();
  } else if (addr < 0x8000) {
    return M->bat[addr - BAT_OFFSET];
  } else if (addr < 0xC000) {
    return M->cart[M->current_bank][addr - BANK_OFFSET];
  } else {
    return M->cart[M->fixed_bank][addr - FIXED_BANK_OFFSET];
  }
}

/*
 * Takes in two addressing bytes, a value, and a memory mapper.
 * Uses the addressing bytes to write the value to the mapper.
 *
 * Assumes that the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 * Assumes that the address formed by the addressing bytes is valid.
 */
void uxrom_write(word_t val, word_t mem_lo, word_t mem_hi, void *map) {
  // Cast back from generic pointer to the memory structure.
  uxrom_t *M = (uxrom_t*) map;

  // Detect where in memory we need to access and do so.
  dword_t addr = get_dword(mem_lo, mem_hi);
  if (addr < 0x2000) {
    M->ram[addr % RAM_SIZE] = val;
    return;
  } else if (addr < 0x4000) {
    M->ppu[(addr - PPU_OFFSET) % PPU_SIZE] = val;
    return;
  } else if (addr < 0x4020) {
    M->io[addr - IO_OFFSET] = val;
    return;
  } else if (addr < 0x6000) {
    printf("FATAL: Memory not implemented");
    abort();
  } else if (addr < 0x8000) {
    M->bat[addr - BAT_OFFSET] = val;
    return;
  } else {
    // Writing to the cart area uses the low bits to select a bank.
    M->current_bank = val & BANK_MASK;
    return;
  }
}

/*
 * Takes in a uxrom memory structure and frees it.
 *
 * Assumes that the fixed bank is the final allocated bank.
 * Assumes the input pointer is a uxrom memory structure and is non-null.
 */
void uxrom_free(void *map) {
  uxrom_t *M = (uxrom_t*) map;

  // TODO: This should change with implementation.
  // Free the contents of the structure.
  free(M->ram);
  free(M->ppu);
  free(M->io);
  free(M->bat);
  free(M->header);

  // Frees each bank.
  for (size_t i = 0; i < (M->fixed_bank + 1U); i++) {
    free(M->cart[i]);
  }

  // Free the structure itself.
  free(M);

  return;
}
