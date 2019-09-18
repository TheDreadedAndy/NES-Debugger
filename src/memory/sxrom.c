/*
 * Implementation of INES Mapper 1 (SxROM).
 *
 * TODO
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./header.h"
#include "./sxrom.h"
#include "../util/data.h"

// Constants used to size and access memory.
#define MAX_ROM_BANK_PLANES 2
#define MAX_ROM_BANKS 16U
#define ROM_BANK_SIZE 0x4000U
#define MAX_RAM_BANKS 4U
#define RAM_BANK_SIZE 0x2000U
#define INES_RAM_SIZE 0x8000U

// Constants used to control accesses to memory.
#define SHIFT_BASE 0x10U

// Constants used to size and access VRAM.
#define MAX_CHR_BANKS 8U
#define CHR_BANK_SIZE 0x1000U
#define MAX_SCREENS 4U
#define SCREEN_SIZE 0x400U

// Nes virtual memory data structure for sxrom (mapper 2).
typedef struct sxrom {
  // Cart memory.
  word_t *prg_rom[MAX_ROM_BANK_PLANES][MAX_ROM_BANKS];
  word_t *prg_ram[MAX_RAM_BANKS];
  word_t num_prg_ram_banks;

  // PPU memory.
  word_t *pattern_table[MAX_CHR_BANKS];
  bool is_chr_ram;
  word_t *nametable[MAX_SCREENS];

  // Controlling registers.
  word_t shift;
  word_t control;
  word_t chr_bank_a;
  word_t chr_bank_b;
  word_t prg_rom_bank;
  word_t prg_ram_bank;
  word_t bus;
} sxrom_t;

/* Helper functions */
void sxrom_load_prg_ram(memory_t *M);
void sxrom_load_prg_rom(FILE *rom_file, memory_t *M);
void sxrom_load_chr(FILE *rom_file, memory_t *M);

/*
 * Uses the header within the provided memory structure to create
 * an sxrom mapper structure and load the game data into the mapper.
 * The mapper structure and its functions are then stored within
 * the provided memory structure.
 *
 * Assumes the provided memory structure and rom file are non-null and valid.
 */
void sxrom_new(FILE *rom_file, memory_t *M) {
  // Allocate memory structure and set up its data.
  sxrom_t *map = xcalloc(1, sizeof(sxrom_t));
  M->map = (void*) map;
  M->read = &sxrom_read;
  M->write = &sxrom_write;
  M->vram_read = &sxrom_vram_read;
  M->vram_write = &sxrom_vram_write;
  M->free = &sxrom_free;

  // Set up the cart ram space.
  sxrom_load_prg_ram(M);

  // Load the rom data into memory.
  sxrom_load_prg_rom(rom_file, M);
  sxrom_load_chr(rom_file, M);

  // Setup the nametable in vram. The header mirroring bit is ignored in
  // this mapper, so we set the mirrored banks to a default value for now.
  map->nametable[0] = rand_alloc(sizeof(word_t) * SCREEN_SIZE);
  map->nametable[3] = rand_alloc(sizeof(word_t) * SCREEN_SIZE);
  map->nametable[1] = map->nametable[0];
  map->nametable[2] = map->nametable[3];

  return;
}

/*
 * Creates a ram space with the amount of memory requested by the mapper.
 * If an INES header is used, the size is not defined and is assumed to be
 * 32K.
 *
 * Assumes the memory structure and its mapper field are non-null.
 * Assumes the header provided by the memory structure is valid.
 */
void sxrom_load_prg_ram(memory_t *M) {
  // Cast back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;

  // Determine if the rom supplied a valid PRG-RAM size.
  if (M->header->header_type != NES2) {
    // Assume that the rom needs 32KB of PRG-RAM.
    map->num_prg_ram_banks = MAX_RAM_BANKS;
  } else {
    // The mapper is NES 2.0, so we can use the specified PRG-RAM size.
    map->num_prg_ram_banks = M->header->prg_ram_size / RAM_BANK_SIZE;
  }

  // Allocate the PRG-RAM banks.
  for (size_t i = 0; i < map->num_prg_ram_banks; i++) {
    map->prg_ram[i] = rand_alloc(RAM_BANK_SIZE * sizeof(word_t));
  }

  return;
}

/*
 * Loads the given rom file into the sxrom memory structure.
 * Note that while this implementation assumes the banks will boot in mode 3,
 * with the upper half of rom fixed to the last bank and the lower half on bank
 * 0, this is not specified in original hardware.
 *
 * Assumes the provided rom_file is valid.
 * Assumes the provided memory structure and its mapper field are non-null.
 * Assumes the mapper field points to an sxrom mapper structure.
 */
void sxrom_load_prg_rom(FILE *rom_file, memory_t *M) {
  // Cast back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;
  (void)map;
  (void)rom_file;

  return;
}

/*
 * TODO
 */
void sxrom_load_chr(FILE *rom_file, memory_t *M) {
  // Cast the mapper back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;
  (void)map;
  (void)rom_file;

  return;
}

/*
 * TODO
 */
word_t sxrom_read(dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)addr;

  return 0;
}

/*
 * TODO
 */
void sxrom_write(word_t val, dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)val;
  (void)addr;

  return;
}

/*
 * TODO
 */
word_t sxrom_vram_read(dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)addr;

  return 0;
}

/*
 * TODO
 */
void sxrom_vram_write(word_t val, dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)val;
  (void)addr;

  return;
}

/*
 * TODO
 */
void sxrom_free(void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;

  return;
}
