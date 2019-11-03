#ifndef _NES_MEM
#define _NES_MEM

#include <cstdlib>
#include <cstdint>

#include "../util/data.h"
#include "./header.h"

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x01U
#define MEMORY_IRQ_ADDR 0xFFFEU
#define MEMORY_RESET_ADDR 0xFFFCU
#define MEMORY_NMI_ADDR 0xFFFAU

// CPU memory map offsets.
#define PPU_OFFSET 0x2000U
#define IO_OFFSET 0x4000U
#define MAPPER_OFFSET 0x4020U

// CPU memory accessing masks.
#define RAM_MASK 0x7FFU
#define BAT_MASK 0x1FFFU

// CPU memory size values.
#define RAM_SIZE 0x800U

// PPU memory map offsets.
#define NAMETABLE_OFFSET 0x2000U
#define PALETTE_OFFSET 0x3F00U

// PPU memory accessing masks.
#define VRAM_BUS_MASK 0x3FFFU
#define NAMETABLE_SELECT_MASK 0x0C00U
#define NAMETABLE_ADDR_MASK 0x03FFU
#define PALETTE_ADDR_MASK 0x001FU
#define PALETTE_BG_ACCESS_MASK 0x0003U
#define PALETTE_BG_MASK 0x000CU

// PPU memory size values.
#define NAMETABLE_SIZE 0x0400U

// Used to interact with the optimized palette.
#define PALETTE_NES_PIXEL_SHIFT 24U
#define PALETTE_XRGB_MASK 0x00FFFFFFU

/*
 * Forward declarations for the other chip emulations.
 *
 * This must be done here because the Memory class does not require these
 * classes on creation. These classes are dependent on each other, with
 * Memory being provided pointers to its associated objects after they have
 * been created (using the Connect() method).
 */
class Cpu;
class Ppu;
class Apu;

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
    RomHeader *header_;

    // Holds the current palette in memory, stored in an xRGB format with
    // the high byte set to the NES color byte. All implementations of memory
    // must fill this array with the current palette data using PaletteWrite.
    uint32_t *palette_data_;
    void PaletteWrite(DoubleWord addr, DataWord val);

    // Points to the associated Ppp/Cpu/Apu classes for this memory class.
    // Necessary to perform most MMIO opperations.
    Cpu *cpu_;
    Ppu *ppu_;
    Apu *apu_;

    // Stores the rom header and allocates the palette data array.
    Memory(RomHeader *header);

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
    void Connect(Cpu *cpu, Ppu *ppu, Apu *apu);

    // Creates a derived memory object for the mapper of the given
    // rom file. Returns NULL on failure.
    Memory *Create(FILE *rom_file);

    // Frees the rom header and palette data array.
    virtual ~Memory(void);
};

#endif
