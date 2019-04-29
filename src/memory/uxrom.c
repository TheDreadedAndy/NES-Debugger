/*
 * TODO.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./uxrom.h"

// TODO: Ensure this setup is complient with all uxrom systems,
// not just mapper 2.

/*
 * Takes in an INES/NES2 header and creates a uxrom
 * memory system using it. Assumes the header is valid.
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
  map->RAM = xcalloc(RAM_SIZE, sizeof(uint8_t));
  // TODO: change these into actual mappings.
  map->PPU = xcalloc(PPU_SIZE, sizeof(uint8_t));
  map->IO = xcalloc(IO_SIZE, sizeof(uint8_t));
  map->bat = xcalloc(MAP2_BAT_SIZE, sizeof(uint8_t));

  // Caclulate rom size and load it into memory.
  size_t numBanks = (size_t)M->header[INES_PRGROM];
  for (size_t i = 0; i < numBanks; i++) {
    map->cart[i] = xmalloc(MAP2_BANK_SIZE * sizeof(uint8_t));
    for (size_t j = 0; j < MAP2_BANK_SIZE; j++) {
      map->cart[i][j] = fgetc(rom);
    }
  }
  map->currentBank = 0;
  map->fixedBank = numBanks - 1;

  return M;
}

// TODO: Fix below.

// Reads memory from a memory structure.
// Only supports mapper 2.
uint8_t uxrom_read(uint8_t locL, uint8_t locH, void *map) {
  // The input map is generic for simplification in memory.c/h, we
  // need to bring it back to the proper structure to use it.
  uxrom_t *M = (uxrom_t*) map;

  uint16_t addr = (((uint16_t)locH) << 8) | locL;
  // Detect where in memory we need to access and do so.
  if (addr < 0x2000) {
    return M->RAM[addr];
  } else if (addr < 0x4000) {
    return M->PPU[(addr - PPU_OFFSET) % PPU_SIZE];
  } else if (addr < 0x4020) {
    return M->IO[addr - IO_OFFSET];
  } else if (addr < 0x6000) {
    printf("FATAL: Memory not implemented");
    abort();
  } else if (addr < 0x8000) {
    return M->bat[addr - MAP2_BAT_OFFSET];
  } else if (addr < 0xC000) {
    return M->cart[M->currentBank][addr - MAP2_BANK_OFFSET];
  } else {
    return M->cart[M->fixedBank][addr - MAP2_FIXED_BANK_OFFSET];
  }
}

// Writes memory to the memory structure.
// Handles bank switches.
void uxrom_write(uint8_t val, uint8_t locL, uint8_t locH, void *map) {
  uxrom_t *M = (uxrom_t*) map;

  uint16_t addr = (((uint16_t)locH) << 8) | locL;
  // Detect where in memory we need to access and do so.
  if (addr < 0x2000) {
    M->RAM[addr] = val;
    return;
  } else if (addr < 0x4000) {
    M->PPU[(addr - PPU_OFFSET) % PPU_SIZE] = val;
    return;
  } else if (addr < 0x4020) {
    M->IO[addr - IO_OFFSET] = val;
    return;
  } else if (addr < 0x6000) {
    printf("FATAL: Memory not implemented");
    abort();
  } else if (addr < 0x8000) {
    M->bat[addr - MAP2_BAT_OFFSET] = val;
    return;
  } else if (addr < 0xC000) {
    // Writing to the cart area uses the low bits to select a bank.
    M->currentBank = val & 0x0f;
    return;
  } else {
    M->currentBank = val & 0x0f;
    return;
  }
}

// Frees a memory structure.
void uxrom_free(void *map) {
  uxrom_t *M = (uxrom_t*) map;

  // TODO: This should change with implementation.
  free(M->RAM);
  free(M->PPU);
  free(M->IO);
  free(M->bat);
  free(M->header);

  // Frees each bank.
  // TODO: This only works for mapper 2.
  for (size_t i = 0; i < (M->fixedBank + 1U); i++) {
    free(M->cart[i]);
  }

  free(M);
}
