// Standard includes
#include <stdlib.h>
#include <stdint.h>

#ifndef _NES_MEM
#define _NES_MEM

// INES header constants.
#define INES_PRGROM 4

// Function types, used in memory structure to point to the proper
// mapper function.
typedef uint8_t memory_read_t(uint8_t locL, uint8_t locH, void *map);
typedef void memory_write_t(uint8_t val, uint8_t locL, uint8_t locH, void *map);
typedef void memory_free_t(void *map);

// Generic memory data structure.
// Includes a pointer to a specific memory implementation and
// the function necessary to interact with said implementation.
typedef struct memory {
  void *map;
  memory_read_t *read;
  memory_write_t *write;
  memory_free_t *free;
  char *header;
} memory_t;

/* Tools for using NES virtual memory */

// Memory data structure creation function. Handles memory maps.
memory_t *memory_new(char *file);

// Generic memory read function.
uint8_t memory_read(uint8_t locL, uint8_t locH, memory_t *M);

// Generic memory write function.
void memory_write(uint8_t val, uint8_t locL, uint8_t locH, memory_t *M);

// Generic memory free function.
void memory_free(memory_t *M);

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x10
#define MEMORY_IRQ_LOW 0xFE
#define MEMORY_IRQ_HIGH 0xFF
#define MEMORY_RESET_LOW 0xFC
#define MEMORY_RESET_HIGH 0xFF
#define MEMORY_NMI_LOW 0xFA
#define MEMORY_NMI_HIGH 0xFF

#endif
