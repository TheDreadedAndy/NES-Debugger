#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"

#ifndef _NES_UXROM
#define _NES_UXROM

// Constants used to size and access memory.
#define MAP2_MAX_BANKS 16
#define MAP2_BANK_SIZE ((size_t)(1 << 14))
#define MAP2_BANK_OFFSET (size_t)(0x8000)
#define MAP2_FIXED_BANK_OFFSET (size_t)(0xC000)
#define MAP2_BAT_SIZE (size_t)(0x2000)
#define MAP2_BAT_OFFSET (size_t)(0x6000)
#define RAM_SIZE ((size_t)(1 << 11))
#define PPU_SIZE (size_t)(0x8)
#define PPU_OFFSET (size_t)(0x2000)
#define IO_SIZE (size_t)(0x20)
#define IO_OFFSET (size_t)(0x4000)
#define HEADER_SIZE (size_t)(0x10)

// Nes virtual memory data structure for uxrom (mapper 2).
typedef struct uxrom {
  uint8_t *RAM;
  uint8_t *PPU;
  uint8_t *IO;
  uint8_t *bat;
  uint8_t *header;
  uint8_t *cart[MAP2_MAX_BANKS];
  uint8_t currentBank;
  // Should always be the final used bank.
  uint8_t fixedBank;
} uxrom_t;

/* Tools for using NES virtual memory */

// Memory data structure creation function.
memory_t *uxrom_new(char *header, FILE *rom);

// Memory read function.
uint8_t uxrom_read(uint8_t locL, uint8_t locH, void *map);

// Memory write function. Handles bank switching.
void uxrom_write(uint8_t val, uint8_t locL, uint8_t locH, void *map);

// Memory free function.
void uxrom_free(void *map);

#endif
