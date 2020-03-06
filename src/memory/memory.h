#ifndef _NES_MEM
#define _NES_MEM

#include <cstdlib>
#include <cstdint>

#include "../util/data.h"
#include "../sdl/input.h"
#include "../io/controller.h"
#include "../config/config.h"
#include "./palette.h"
#include "./header.h"

// Memory addressing constants.
#define MEMORY_STACK_HIGH 0x01U
#define MEMORY_VECTOR_LOW 0xFAU
#define MEMORY_VECTOR_HIGH 0xFFU
#define MEMORY_IRQ_ADDR 0xFFFEU
#define MEMORY_RESET_ADDR 0xFFFCU
#define MEMORY_NMI_ADDR 0xFFFAU
#define PPU_OAM_ADDR 0x2004U

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

// PPU memory size values.
#define NAMETABLE_SIZE 0x0400U

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
  private:
    // The decoded palette, which is used to convert NES pixels to pixel format
    // used by the emulation.
    NesPalette *palette_;

    // The palette array holds the NES representation of the palette, exposed
    // to the emulated software. The pixel data array mirrors the palette
    // in the pixel format of the emulator.
    PixelPalette *pixels_;

  protected:
    // Points the header structure asssociated with the loaded rom.
    RomHeader *header_;

    // Points to the associated Ppp/Cpu/Apu classes for this memory class.
    // Necessary to perform most MMIO opperations.
    Cpu *cpu_;
    Ppu *ppu_;
    Apu *apu_;

    // The controller connected to this memory object.
    Controller *controller_ = NULL;

    // Stores the rom header and allocates the palette data array.
    Memory(RomHeader *header, Config *config);

    // All implementations of memory should access the palette only through
    // these helper functions.
    DataWord PaletteRead(DoubleWord addr);
    void PaletteWrite(DoubleWord addr, DataWord val);

  public:
    // Provides access to CPU memory.
    virtual DataWord Read(DoubleWord addr) = 0;
    virtual void Write(DoubleWord addr, DataWord val) = 0;

    // Checks if an access to CPU memory will have side effects outside the CPU.
    virtual bool CheckRead(DoubleWord addr) = 0;
    virtual bool CheckWrite(DoubleWord addr) = 0;

    // Provides access to PPU memory.
    virtual DataWord VramRead(DoubleWord addr) = 0;
    virtual void VramWrite(DoubleWord addr, DataWord val) = 0;

    // Allows the PPU to update the current mask setting of the palette.
    void PaletteUpdate(DataWord mask);

    // Exposes the decoded palette to the PPU.
    // The exposed data must not be modified.
    const PixelPalette *PaletteExpose(void);

    // Gives the memory access to its associated Ppu/Cpu classes.
    // Must be called before using r/w functions.
    void Connect(Cpu *cpu, Ppu *ppu, Apu *apu);

    // Uses the given input class to create and connect a controller
    // to the memory object. Must be called before using r/w functions.
    void AddController(Input *input);

    // Creates a derived memory object for the mapper of the given
    // rom file. Returns NULL on failure.
    static Memory *Create(FILE *rom_file, Config *config);

    // Frees the rom header and palette data array.
    virtual ~Memory(void);
};

#endif
