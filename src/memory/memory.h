#include <stdlib.h>
#include <stdint.h>
#include "../util/data.h"
#include "./header.h"
#include "../ppu/ppu.h"
#include "../cpu/2A03.h"

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
 * As an optimization, each mapper must fill a palette array that allows the
 * PPU to retrieve the xRGB color associated with any color in palette memory.
 *
 * All memory classes are given access to the header structure associated with
 * the loaded rom, should they require it in their implementation.
 *
 * All memory classes must be connected to valid CPU, PPU, and APU classes
 * before they can be used.
 */
class Memory {
  protected:
    // Points the header structure asssociated with the loaded rom.
    header_t *header;

    // Holds the current palette in memory, stored in an xRGB format with
    // the high byte set to the NES color byte. All implementations of memory
    // must fill this array with the current palette data using PaletteWrite.
    uint32_t *palette_data;
    void PaletteWrite(DoubleWord addr, DataWord val);

    // Points to the associated Ppp/Cpu/Apu classes for this memory class.
    // Necessary to perform most MMIO opperations.
    Cpu *cpu;
    Ppu *ppu;
    Apu *apu;

  public:
    // Provides access to CPU memory.
    virtual DataWord Read(DoubleWord addr) = 0;
    virtual void Write(DoubleWord addr, DataWord val) = 0;

    // Provides access to PPU memory.
    virtual DataWord VramRead(DoubleWord addr) = 0;
    virtual void VramWrite(DoubleWord addr, DataWord val) = 0;

    // Provides quick access to the colors in the palette.
    uint32_t PaletteRead(DoubleWord addr);
    void PaletteUpdate();

    // Gives the memory access to its associated Ppu/Cpu classes.
    // Must be called before any other function.
    virtual void Connect(Cpu *conn_cpu, Ppu *conn_ppu, Apu *conn_apu) = 0;

    // Decodes the header of the given rom and creates the palette data array.
    virtual Memory();

    // Frees the header and palette data array.
    virtual ~Memory();
};

// Memory data structure initialization function. Handles memory maps.
Memory *MemoryCreate(FILE *rom_file);

#endif
