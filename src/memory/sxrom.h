#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"
#include "./header.h"
#include "../util/data.h"

#ifndef _NES_SXROM
#define _NES_SXROM

// The defined mapper number that this memory system belongs to.
#define SXROM_MAPPER 1

/* Tools for using NES virtual memory */

// Memory data structure creation function.
void sxrom_new(FILE *rom_file, memory_t *M);

// Memory read function.
word_t sxrom_read(dword_t addr, void *map);

// Memory write function. Handles bank switching.
void sxrom_write(word_t val, dword_t addr, void *map);

// VRAM read function.
word_t sxrom_vram_read(dword_t addr, void *map);

// VRAM write function.
void sxrom_vram_write(word_t val, dword_t addr, void *map);

// Memory free function.
void sxrom_free(void *map);

#endif
