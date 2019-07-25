#include <stdlib.h>
#include <stdint.h>
#include "../util/data.h"
#include "./header.h"

#ifndef _NES_MEM
#define _NES_MEM

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x10U
#define MEMORY_IRQ_LOW 0xFEU
#define MEMORY_IRQ_HIGH 0xFFU
#define MEMORY_RESET_LOW 0xFCU
#define MEMORY_RESET_HIGH 0xFFU
#define MEMORY_NMI_LOW 0xFAU
#define MEMORY_NMI_HIGH 0xFFU

// Function types, used in memory structure to point to the proper
// mapper function.
typedef word_t memory_read_t(dword_t addr, void *map);
typedef void memory_write_t(word_t val, dword_t addr, void *map);
typedef void memory_free_t(void *map);

// Generic memory data structure.
// Includes a pointer to a specific memory implementation and
// the function necessary to interact with said implementation.
typedef struct memory {
  // NES system RAM and VRAM palette data.
  // Mappers do not change this or any MMIO.
  word_t *ram;
  word_t *palette_data;

  // Mapper information.
  void *map;
  memory_read_t *read;
  memory_write_t *write;
  memory_read_t *vram_read;
  memory_write_t *vram_write;
  memory_free_t *free;
  header_t *header;
} memory_t;

/* Tools for using NES virtual memory */

// Memory data structure initialization function. Handles memory maps.
bool memory_init(FILE *rom_file, header_t *header);

// Generic memory read function.
word_t memory_read(word_t mem_lo, word_t mem_hi);

// Generic memory write function.
void memory_write(word_t val, word_t mem_lo, word_t mem_hi);

// Generic vram read function.
word_t memory_vram_read(dword_t addr);

// Generic vram write function.
void memory_vram_write(word_t val, dword_t addr);

// Generic memory free function.
void memory_free(void);

#endif
