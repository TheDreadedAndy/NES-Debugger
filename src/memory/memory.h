#include <stdlib.h>
#include <stdint.h>
#include "../util/data.h"
#include "./header.h"

#ifndef _NES_MEM
#define _NES_MEM

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x01U
#define MEMORY_IRQ_ADDR 0xFFFEU
#define MEMORY_RESET_ADDR 0xFFFCU
#define MEMORY_NMI_ADDR 0xFFFAU

/*
 * Abstract memory class. All memory mappers must implement these functions.
 *
 * The read and write functions allow access to the CPU's memory.
 * The vram functions allow access to the PPU's memory.
 * As an optimization, each mapper must provide someway to retrieve the
 * xRGB color associated with any color in palette data.
 *
 * All memory classes are given access to the header structure associated with
 * the loaded rom, should they require it in their implementation.
 */
class memory_t {
  protected:
    // Points the header structure asssociated with the loaded rom.
    header_t *header;

  public:
    // Provides access to CPU memory.
    virtual word_t read(dword_t addr) = 0;
    virtual void write(dword_t addr, word_t val) = 0;

    // Provides access to PPU memory.
    virtual word_t vram_read(dword_t addr) = 0;
    virtual void vram_write(dword_t addr, word_t val) = 0;

    // Provides quick access to the colors in the palette.
    virtual uint32_t palette_read(dword_t addr) = 0;
    virtual void palette_update() = 0;

    // Frees any structures used by the memory class.
    virtual ~memory() = 0;
};

// The last value read/written to memory. Used to emulate open-bus behavior.
extern word_t memory_bus;

/* Tools for using NES virtual memory */

// Memory data structure initialization function. Handles memory maps.
bool memory_init(FILE *rom_file, header_t *header);

// Generic memory read function.
word_t memory_read(dword_t addr);

// Generic memory write function.
void memory_write(word_t val, dword_t addr);

// Generic vram read function.
word_t memory_vram_read(dword_t addr);

// Generic vram write function.
void memory_vram_write(word_t val, dword_t addr);

// Reads an xRGB color from palette ram.
uint32_t memory_palette_read(dword_t addr);

// Refreshes the xRGB colors stored in palette ram.
void memory_palette_update(void);

// Generic memory free function.
void memory_free(void);

#endif
