#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"
#include "./header.h"
#include "../util/data.h"

#ifndef _NES_UXROM
#define _NES_UXROM

// The defined mapper number that this memory system belongs to.
#define UXROM_MAPPER 2

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
