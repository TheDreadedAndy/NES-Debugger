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
#include "./header.h"
#include "./uxrom.h"
#include "../util/data.h"

/*
 * Stores the mirror bit collected from the header. Used to change mirroring
 * in VRAM.
 */
bool name_table_mirror;

/*
 * Takes in an INES/NES2 header and creates a uxrom
 * memory system using it. Assumes the header is valid.
 *
 * Returns a generic memory structure, which can be used
 * to interact with the uxrom structure.
 */
memory_t *uxrom_new(FILE *rom_file, header_t *header) {
  // Allocate memory structure and set up its data.
  memory_t *M = xcalloc(1, sizeof(memory_t));
  uxrom_t *map = xcalloc(1, sizeof(uxrom_t));
  M->map = (void*) map;
  M->read = &uxrom_read;
  M->write = &uxrom_write;
  M->vram_read = &uxrom_vram_read;
  M->vram_write = &uxrom_vram_write;
  M->free = &uxrom_free;
  M->header = header;

  // Store the mirror bit for use with VRAM.
  name_table_mirror = header->mirror;

  // Set up the cart ram space.
  map->bat = xcalloc(BAT_SIZE, sizeof(word_t));

  // Caclulate rom size and load it into memory.
  size_t num_banks = (size_t) (header->prg_rom_size / (1 << 14));
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    map->cart[i] = xmalloc(BANK_SIZE * sizeof(word_t));
    for (size_t j = 0; j < BANK_SIZE; j++) {
      map->cart[i][j] = fgetc(rom_file);
    }
  }
  map->current_bank = 0;
  map->fixed_bank = num_banks - 1;

  return M;
}

/*
 * Takes in an address and a generic mapper pointer.
 * Uses these bytes to address the memory in the mapper.
 *
 * Assumes that the pointer is non-null and points to a valid
 * UxROM mapper.
 */
word_t uxrom_read(dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  uxrom_t *M = (uxrom_t*) map;

  // Detect where in memory we need to access and do so.
  if (addr < 0x6000) {
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
 * Takes in an address, a value, and a memory mapper.
 * Uses the address to write the value to the mapper.
 *
 * Assumes that the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
void uxrom_write(word_t val, dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  uxrom_t *M = (uxrom_t*) map;

  // Detect where in memory we need to access and do so.
  if (addr < 0x6000) {
    printf("FATAL: Memory not implemented");
    abort();
  } else if (addr < 0x8000) {
    M->bat[addr - BAT_OFFSET] = val;
  } else {
    // Writing to the cart area uses the low bits to select a bank.
    M->current_bank = val & BANK_MASK;
  }

  return;
}

/*
 * TODO
 */
word_t uxrom_vram_read(dword_t addr, void *map) {
  (void) addr;
  (void) map;
  return 0;
}

/*
 * TODO
 */
void uxrom_vram_write(word_t val, dword_t addr, void *map) {
  (void) val;
  (void) addr;
  (void) map;
  return;
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
  free(M->bat);

  // Frees each bank.
  for (size_t i = 0; i < (M->fixed_bank + 1U); i++) {
    free(M->cart[i]);
  }

  // Free the structure itself.
  free(M);

  return;
}
