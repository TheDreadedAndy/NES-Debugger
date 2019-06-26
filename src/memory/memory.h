#include <stdlib.h>
#include <stdint.h>
#include "../util/data.h"
#include "./header.h"

#ifndef _NES_MEM
#define _NES_MEM

// INES header constants.
#define INES_PRGROM 4

// Memory data constants.
#define RAM_SIZE 0x800U

// Function types, used in memory structure to point to the proper
// mapper function.
typedef word_t memory_read_t(word_t mem_lo, word_t mem_hi, void *map);
typedef void memory_write_t(word_t val, word_t mem_lo,
                            word_t mem_hi, void *map);
typedef void memory_free_t(void *map);

// Generic memory data structure.
// Includes a pointer to a specific memory implementation and
// the function necessary to interact with said implementation.
typedef struct memory {
  void *map;
  memory_read_t *read;
  memory_write_t *write;
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

// Generic memory free function.
void memory_free();

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x10U
#define MEMORY_IRQ_LOW 0xFEU
#define MEMORY_IRQ_HIGH 0xFFU
#define MEMORY_RESET_LOW 0xFCU
#define MEMORY_RESET_HIGH 0xFFU
#define MEMORY_NMI_LOW 0xFAU
#define MEMORY_NMI_HIGH 0xFFU

#endif
