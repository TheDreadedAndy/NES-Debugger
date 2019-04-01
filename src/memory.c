/*
 * NES memory implementation.
 * Abstracts away mappers and memory mapping from the 2A03.
 * Loading the rom and dealing with it's mapper are handled
 * in this file. The NES2/INES header are parsed in memory_new().
 * Currently only supports mapper 2.
 */

#include <stdlib.h>
#include <stdint.h>
#include "util.h"

memory_t *memory_new(char *file) {
  memory_t *M = xcalloc(1, sizeof(memory_t));
  M->RAM = xcalloc(RAM_SIZE, sizeof(uint8_t));
  // TODO: change these into actual mappings.
  M->PPU = xcalloc(PPU_SIZE, sizeof(uint8_t));
  M->IO = xcalloc(IO_SIZE, sizeof(uint8_t));
  M->bat = xcalloc(BAT_SIZE, sizeof(uint8_t));

  return M;
}

// Reads memory from a memory structure.
// Only supports mapper 2.
uint8_t memory_read(uint8_t locL, uint8_t locH, memory_t *M) {
  uint16_t addr = (((uint16_t)locH) << 8) | locH;
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
void memory_write(uint8_t val, uint8_t locL, uint8_t locH, memory_t *M) {
  uint16_t addr = (((uint16_t)locH) << 8) | locH;
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
    M->currentBank = val & 0x0f
    return;
  }
}
