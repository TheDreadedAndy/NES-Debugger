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

// Constants used to size and access memory.
#define MAX_BANKS 16U
#define BANK_SIZE 0x4000U
#define BANK_OFFSET 0x8000U
#define BANK_ADDR_MASK 0x3FFFU
#define UOROM_BANK_MASK 0x0FU
#define UNROM_BANK_MASK 0x07U
#define MAX_UNROM_BANKS 8
#define FIXED_BANK_OFFSET 0xC000U
#define BAT_SIZE 0x2000U
#define BAT_OFFSET 0x6000U
#define BAT_MASK 0x1FFFU

// Constants used to size and access VRAM.
#define PATTERN_TABLE_SIZE 0x2000U
#define NAMETABLE_SIZE 0x0400U
#define MAX_SCREENS 4U
#define NAMETABLE_ACCESS_BIT 0x2000U
#define NAMETABLE_SELECT_MASK 0x0C00U
#define NAMETABLE_ADDR_MASK 0x03FFU
#define PATTERN_TABLE_MASK 0x1FFFU


// Nes virtual memory data structure for uxrom (mapper 2).
typedef struct uxrom {
  // Cart memory.
  word_t *bat;
  word_t *cart[MAX_BANKS];
  word_t current_bank;
  // Should always be the final used bank.
  word_t fixed_bank;
  word_t bank_mask;

  // PPU memory.
  word_t *pattern_table;
  bool is_chr_ram;
  word_t *nametable[MAX_SCREENS];
} uxrom_t;

/* Helper functions */
void uxrom_load_prg(FILE *rom_file, memory_t *M);
void uxrom_load_chr(FILE *rom_file, memory_t *M);

/*
 * Uses the header within the provided memory structure to create
 * a uxrom mapper structure and load the game data into the mapper.
 * The mapper structure and its functions are then stored within
 * the provided memory structure.
 *
 * Assumes the provided memory structure and rom file are non-null and valid.
 */
void uxrom_new(FILE *rom_file, memory_t *M) {
  // Allocate memory structure and set up its data.
  uxrom_t *map = xcalloc(1, sizeof(uxrom_t));
  M->map = (void*) map;
  M->read = &uxrom_read;
  M->write = &uxrom_write;
  M->vram_read = &uxrom_vram_read;
  M->vram_write = &uxrom_vram_write;
  M->free = &uxrom_free;

  // Set up the cart ram space.
  map->bat = xcalloc(BAT_SIZE, sizeof(word_t));

  // Load the rom data into memory.
  uxrom_load_prg(rom_file, M);
  uxrom_load_chr(rom_file, M);

  // Setup the nametable in vram.
  map->nametable[0] = xcalloc(sizeof(word_t), NAMETABLE_SIZE);
  map->nametable[3] = xcalloc(sizeof(word_t), NAMETABLE_SIZE);
  if (M->header->mirror) {
    // Vertical (horizontal arrangement) mirroring.
    map->nametable[1] = map->nametable[3];
    map->nametable[2] = map->nametable[0];
  } else {
    // Horizontal (vertical arrangement) mirroring.
    map->nametable[1] = map->nametable[0];
    map->nametable[2] = map->nametable[3];
  }

  return;
}

/*
 * Loads the prg section of a ROM into the mapped memory system.
 *
 * Assumes the mapper structure has been allocated and is of type uxrom_t.
 * Assumes the header within the memory structure is valid.
 */
void uxrom_load_prg(FILE *rom_file, memory_t *M) {
  // Cast back from the generic structure.
  uxrom_t *map = (uxrom_t*) M->map;

  // Calculate the number of prg banks used by the rom, then load
  // the rom into memory.
  size_t num_banks = (size_t) (M->header->prg_rom_size / BANK_SIZE);
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    map->cart[i] = xmalloc(BANK_SIZE * sizeof(word_t));
    for (size_t j = 0; j < BANK_SIZE; j++) {
      map->cart[i][j] = fgetc(rom_file);
    }
  }

  // Setup the default banks and max banks.
  map->current_bank = 0;
  map->fixed_bank = num_banks - 1;
  map->bank_mask = (num_banks > MAX_UNROM_BANKS)
                 ? UOROM_BANK_MASK : UNROM_BANK_MASK;

  return;
}

/*
 * Loads the chr section of a ROM into the mapped memory system.
 * If the size of the chr rom is 0, chr ram is created instead.
 *
 * Assumes the mapper structure has been allocated and is of type uxrom_t.
 * Assumes the header within the memory structure is valid.
 */
void uxrom_load_chr(FILE *rom_file, memory_t *M) {
  // Cast the mapper back from the generic structure.
  uxrom_t *map = (uxrom_t*) M->map;

  // Check if the rom is using chr-ram, and allocate it if so.
  if (M->header->chr_ram_size > 0) {
    map->is_chr_ram = true;
    // chr-ram should always be 8K for this mapper.
    map->pattern_table = xcalloc(sizeof(word_t), M->header->chr_ram_size);
    return;
  }

  // Otherwise, the rom uses chr-rom and the data needs to be copied
  // from the rom file.
  map->is_chr_ram = false;
  map->pattern_table = xcalloc(sizeof(word_t), M->header->chr_rom_size);
  fseek(rom_file, HEADER_SIZE + M->header->prg_rom_size, SEEK_SET);
  for (size_t i = 0; i < M->header->chr_rom_size; i++) {
    map->pattern_table[i] = fgetc(rom_file);
  }

  return;
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
  if (addr < BAT_OFFSET) {
    fprintf(stderr, "FATAL: Memory not implemented.\n");
    abort();
  } else if (addr < BANK_OFFSET) {
    return M->bat[addr & BAT_MASK];
  } else if (addr < FIXED_BANK_OFFSET) {
    return M->cart[M->current_bank][addr & BANK_ADDR_MASK];
  } else {
    return M->cart[M->fixed_bank][addr & BANK_ADDR_MASK];
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
  if (addr < BAT_OFFSET) {
    fprintf(stderr, "FATAL: Memory not implemented.\n");
    abort();
  } else if (addr < BANK_OFFSET) {
    M->bat[addr & BAT_MASK] = val;
  } else {
    // Writing to the cart area uses the low bits to select a bank.
    M->current_bank = val & M->bank_mask;
  }

  return;
}

/*
 * Reads a word from the remappable section of VRAM.
 *
 * Assumes the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
word_t uxrom_vram_read(dword_t addr, void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Name table is being accessed.
    word_t table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    return M->nametable[table][addr & NAMETABLE_ADDR_MASK];
  } else {
    // Pattern table is being accessed.
    return M->pattern_table[addr & PATTERN_TABLE_MASK];
  }
}

/*
 * Writes a word to the remappable section of VRAM.
 * Does nothing if the program attempts to write to CHR-ROM.
 *
 * Assumes the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
void uxrom_vram_write(word_t val, dword_t addr, void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Name table is being accessed.
    word_t table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    M->nametable[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if (M->is_chr_ram) {
    // Pattern table is being accessed.
    M->pattern_table[addr & PATTERN_TABLE_MASK] = val;
  }

  return;
}

/*
 * Takes in a uxrom memory structure and frees it.
 *
 * Assumes that the fixed bank is the final allocated bank.
 * Assumes the input pointer is a uxrom memory structure and is non-null.
 */
void uxrom_free(void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Free the contents of the structure.
  free(M->bat);
  free(M->pattern_table);
  free(M->nametable[0]);
  free(M->nametable[3]);

  // Frees each bank.
  for (size_t i = 0; i < (M->fixed_bank + 1U); i++) {
    free(M->cart[i]);
  }

  // Free the structure itself.
  free(M);

  return;
}
