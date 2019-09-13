/*
 * Implementation of INES Mapper 0 (NROM).
 *
 * This is the most basic NES mapper, with no bank switching.
 *
 * This implementation supports up to 32KB of program data, 8KB of ram,
 * and 8KB of CHR data.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./header.h"
#include "./nrom.h"
#include "../util/data.h"

// Constants used to size and access memory.
#define MAX_BANKS 2U
#define BANK_SIZE 0x4000U
#define BANK_SELECT_MASK 0x4000U
#define BANK_SELECT_SHIFT 14U
#define BANK_OFFSET 0x8000U
#define BANK_ADDR_MASK 0x3FFFU
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


// Nes virtual memory data structure for nrom (mapper 0).
typedef struct nrom {
  // Cart memory.
  word_t bat[BAT_SIZE];
  word_t *cart[MAX_BANKS];

  // PPU memory.
  word_t pattern_table[PATTERN_TABLE_SIZE];
  bool is_chr_ram;
  word_t *nametable[MAX_SCREENS];
} nrom_t;

/* Helper functions */
void nrom_load_prg(FILE *rom_file, memory_t *M);
void nrom_load_chr(FILE *rom_file, memory_t *M);

/*
 * Uses the header within the provided memory structure to create
 * a nrom mapper structure and load the game data into the mapper.
 * The mapper structure and its functions are then stored within
 * the provided memory structure.
 *
 * Assumes the provided memory structure and rom file are non-null and valid.
 */
void nrom_new(FILE *rom_file, memory_t *M) {
  // Allocate memory structure and set up its data.
  nrom_t *map = rand_alloc(sizeof(nrom_t));
  M->map = (void*) map;
  M->read = &nrom_read;
  M->write = &nrom_write;
  M->vram_read = &nrom_vram_read;
  M->vram_write = &nrom_vram_write;
  M->free = &nrom_free;

  // Load the rom data into memory.
  nrom_load_prg(rom_file, M);
  nrom_load_chr(rom_file, M);

  // Setup the nametable in vram.
  map->nametable[0] = rand_alloc(sizeof(word_t) * NAMETABLE_SIZE);
  map->nametable[3] = rand_alloc(sizeof(word_t) * NAMETABLE_SIZE);
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
 * Assumes the mapper structure has been allocated and is of type nrom_t.
 * Assumes the header within the memory structure is valid.
 */
void nrom_load_prg(FILE *rom_file, memory_t *M) {
  // Cast back from the generic structure.
  nrom_t *map = (nrom_t*) M->map;

  // Calculate the number of prg banks used by the rom, then load
  // the rom into memory.
  size_t num_banks = (size_t) (M->header->prg_rom_size / BANK_SIZE);
  //FIXME: Should error if too large.
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    map->cart[i] = xmalloc(BANK_SIZE * sizeof(word_t));
    for (size_t j = 0; j < BANK_SIZE; j++) {
      map->cart[i][j] = fgetc(rom_file);
    }
  }

  // If the cart contains only 16KB of PRG-ROM, we mirror it into
  // the second bank.
  if (M->header->prg_rom_size <= BANK_SIZE) {
    map->cart[1] = map->cart[0];
  }

  return;
}

/*
 * Loads the chr section of a ROM into the mapped memory system.
 * If the size of the chr rom is 0, chr ram is created instead.
 *
 * Assumes the mapper structure has been allocated and is of type nrom_t.
 * Assumes the header within the memory structure is valid.
 */
void nrom_load_chr(FILE *rom_file, memory_t *M) {
  // Cast the mapper back from the generic structure.
  nrom_t *map = (nrom_t*) M->map;

  // Check if the rom is using chr-ram, and allocate it if so.
  if (M->header->chr_ram_size > 0) {
    map->is_chr_ram = true;
    // chr-ram should always be 8K for this mapper.
    return;
  }

  // Otherwise, the rom uses chr-rom and the data needs to be copied
  // from the rom file.
  map->is_chr_ram = false;
  fseek(rom_file, HEADER_SIZE + M->header->prg_rom_size, SEEK_SET);
  for (size_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
    map->pattern_table[i] = fgetc(rom_file);
  }

  return;
}

/*
 * Takes in an address and a generic mapper pointer.
 * Uses these bytes to address the memory in the mapper.
 *
 * Assumes that the pointer is non-null and points to a valid
 * NROM mapper.
 */
word_t nrom_read(dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  nrom_t *M = (nrom_t*) map;

  // Detect where in memory we need to access and do so.
  word_t bank;
  if (addr < BAT_OFFSET) {
    fprintf(stderr, "WARNING: Memory not implemented.\n");
    return 0;
  } else if (addr < BANK_OFFSET) {
    return M->bat[addr & BAT_MASK];
  } else {
    bank = (addr & BANK_SELECT_MASK) >> BANK_SELECT_SHIFT;
    return M->cart[bank][addr & BANK_ADDR_MASK];
  }
}

/*
 * Takes in an address, a value, and a memory mapper.
 * Uses the address to write the value to the mapper.
 *
 * Assumes that the mapper pointer is non-null and points
 * to a valid NROM mapper.
 */
void nrom_write(word_t val, dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  nrom_t *M = (nrom_t*) map;

  // Detect where in memory we need to access and do so.
  if (addr < BAT_OFFSET) {
    fprintf(stderr, "WARNING: Memory not implemented.\n");
  } else if (addr < BANK_OFFSET) {
    M->bat[addr & BAT_MASK] = val;
  }

  return;
}

/*
 * Reads a word from the remappable section of VRAM.
 *
 * Assumes the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
word_t nrom_vram_read(dword_t addr, void *map) {
  // Cast back from the generic structure.
  nrom_t *M = (nrom_t*) map;

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
void nrom_vram_write(word_t val, dword_t addr, void *map) {
  // Cast back from the generic structure.
  nrom_t *M = (nrom_t*) map;

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
 * Takes in a nrom memory structure and frees it.
 *
 * Assumes that the fixed bank is the final allocated bank.
 * Assumes the input pointer is a nrom memory structure and is non-null.
 */
void nrom_free(void *map) {
  // Cast back from the generic structure.
  nrom_t *M = (nrom_t*) map;

  // Free the contents of the structure.
  free(M->nametable[0]);
  free(M->nametable[3]);

  // Frees each bank.
  free(M->cart[0]);
  if (M->cart[1] != M->cart[0]) { free(M->cart[1]); }

  // Free the structure itself.
  free(M);

  return;
}
