#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"
#include "./header.h"
#include "../util/data.h"

#ifndef _NES_UXROM
#define _NES_UXROM

// The defined mapper number that this memory system belongs to.
#define UXROM_MAPPER 2

// Constants used to size and access memory.
#define MAX_BANKS 16U
#define BANK_SIZE ((size_t)(1 << 14))
#define BANK_OFFSET 0x8000U
#define BANK_MASK 0x0f
#define FIXED_BANK_OFFSET 0xC000U
#define BAT_SIZE 0x2000U
#define BAT_OFFSET 0x6000U

// Nes virtual memory data structure for uxrom (mapper 2).
typedef struct uxrom {
  // Cart memory.
  word_t *bat;
  word_t *cart[MAX_BANKS];
  word_t current_bank;
  // Should always be the final used bank.
  word_t fixed_bank;

  // PPU memory.
  word_t *pattern_table;
  word_t *name_table;
  word_t *palette_ram;
} uxrom_t;

/* Tools for using NES virtual memory */

// Memory data structure creation function.
void uxrom_new(FILE *rom_file, memory_t *M);

// Memory read function.
word_t uxrom_read(dword_t addr, void *map);

// Memory write function. Handles bank switching.
void uxrom_write(word_t val, dword_t addr, void *map);

// VRAM read function.
word_t uxrom_vram_read(dword_t addr, void *map);

// VRAM write function.
void uxrom_vram_write(word_t val, dword_t addr, void *map);

// Memory free function.
void uxrom_free(void *map);

#endif
