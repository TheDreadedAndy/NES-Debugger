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

//TODO Remove these
#define PPU_SIZE 0x8U
#define PPU_OFFSET 0x2000U
#define IO_SIZE 0x20U
#define IO_OFFSET 0x4000U

// Nes virtual memory data structure for uxrom (mapper 2).
typedef struct uxrom {
  word_t *ram;
  word_t *ppu;
  word_t *io;
  word_t *bat;
  word_t *cart[MAX_BANKS];
  word_t current_bank;
  // Should always be the final used bank.
  word_t fixed_bank;
} uxrom_t;

/* Tools for using NES virtual memory */

// Memory data structure creation function.
memory_t *uxrom_new(FILE *rom_file, header_t *header);

// Memory read function.
word_t uxrom_read(word_t mem_lo, word_t mem_hi, void *map);

// Memory write function. Handles bank switching.
void uxrom_write(word_t val, word_t mem_lo, word_t mem_hi, void *map);

// Memory free function.
void uxrom_free(void *map);

#endif
