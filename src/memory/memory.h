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
class Memory {
  protected:
    // Points the header structure asssociated with the loaded rom.
    header_t *header;

  public:
    // Provides access to CPU memory.
    virtual word_t Read(dword_t addr) = 0;
    virtual void Write(dword_t addr, word_t val) = 0;

    // Provides access to PPU memory.
    virtual word_t VramRead(dword_t addr) = 0;
    virtual void VramWrite(dword_t addr, word_t val) = 0;

    // Provides quick access to the colors in the palette.
    virtual uint32_t PaletteRead(dword_t addr) = 0;
    virtual void PaletteUpdate() = 0;

    // Frees any structures used by the memory class.
    virtual ~Memory() = 0;
};

// Memory data structure initialization function. Handles memory maps.
Memory *MemoryCreate(FILE *rom_file, header_t *rom_header);

#endif
