#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"
#include "./header.h"
#include "../util/data.h"

#ifndef _NES_NROM
#define _NES_NROM

// The defined mapper number that this memory system belongs to.
#define NROM_MAPPER 0

/* Tools for using NES virtual memory */

// Memory data structure creation function.
void nrom_new(FILE *rom_file, memory_t *M);

// Memory read function.
word_t nrom_read(dword_t addr, void *map);

// Memory write function. Handles bank switching.
void nrom_write(word_t val, dword_t addr, void *map);

// VRAM read function.
word_t nrom_vram_read(dword_t addr, void *map);

// VRAM write function.
void nrom_vram_write(word_t val, dword_t addr, void *map);

// Memory free function.
void nrom_free(void *map);

#endif
