#include <stdlib.h>
#include <stdint.h>

#ifndef _NES_MEM
#define _NES_MEM

// Constants used to size and access memory.
#define MAP2_MAX_BANKS 16
#define MAP2_BANK_SIZE ((size_t)(1 << 14));
#define MAP2_BANK_OFFSET (size_t)(0x8000);
#define MAP2_FIXED_BANK_OFFSET (size_t)(0xC000);
#define MAP2_BAT_SIZE (size_t)(0x2000);
#define MAP2_BAT_OFFSET (size_t)(0x6000);
#define RAM_SIZE ((size_t)(1 << 11));
#define PPU_SIZE (size_t)(0x8);
#define PPU_OFFSET (size_t)(0x2000);
#define IO_SIZE (size_t)(0x20);
#define IO_OFFSET (size_t)(0x4000);
#define HEADER_SIZE (size_t)(0x10);

// INES header constants.
#define INES_PRGROM 4;

// Nes virtual memory data structure.
// For now, I'll only be implementing mapper 2.
typedef struct memory {
  uint8_t *RAM;
  uint8_t *PPU;
  uint8_t *IO;
  uint8_t *bat;
  uint8_t *header;
  uint8_t *cart[MAX_BANKS];
  uint8_t currentBank;
  // Should always be the final used bank.
  uint8_t fixedBank;
} memory_t

/* Tools for using NES virtual memory */

// Memory data structure creation function.
memory_t *memory_new(char *file);

// Memory read function. Handles memory maps.
uint8_t memory_read(uint8_t locL, uint8_t locH, memory_t *M);

// Memory write function. Handles memory maps and bank switching.
void memory_write(uint8_t val, uint8_t locL, uint8_t locH, memory_t *M);

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x10
#define MEMORY_IRQ_LOW 0xFE
#define MEMORY_IRQ_HIGH 0xFF
#define MEMORY_RESET_LOW 0xFC
#define MEMORY_RESET_HIGH 0xFF
#define MEMORY_NMI_LOW 0xFA
#define MEMORY_NMI_HIGH 0xFF

#endif
