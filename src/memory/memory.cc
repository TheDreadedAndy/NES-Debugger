/*
 * This file contains the abstract memory class, which is used to abstract
 * the memory mappings of each game cartridge away from the CPU.
 *
 * Each mapper must define a way to read/write to both vram and cpu memory.
 * Palette access are handled within the Memory class, and need not be
 * implemented by the mappers; the mapper can simple call the memory
 * class's functions when it receives a read/write to palette data.
 *
 * There are about 300 different memory mappers in the iNes/NES 2.0 header
 * standard. As of right now, not all of these are implemented. Each
 * mapper represents a different set of memory asics that were used in
 * original NES cartridges.
 */

#include "./memory.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/util.h"
#include "../util/data.h"
#include "../ppu/ppu.h"
#include "../ppu/palette.h"
#include "../cpu/cpu.h"
#include "../io/controller.h"
#include "../apu/apu.h"
#include "./header.h"
#include "./nrom.h"
#include "./sxrom.h"
#include "./uxrom.h"

// Size of NES palette data.
#define PALETTE_DATA_SIZE 0x20U

/*
 * Decodes the header of the provided rom file, and creates the appropriate
 * memory class for it.
 *
 * On success, the returned value is a memory mapper object cast to Memory.
 * On failure, returns NULL.
 *
 * Assumes the provided rom file is non-null and a valid NES rom.
 */
Memory *Memory::Create(FILE *rom_file) {
  // Use the provided rom file to create a decoded rom header.
  RomHeader *header = DecodeHeader(rom_file);
  if (header == NULL) { return NULL; }

  // Use the decoded header to decide which memory structure should be created.
  Memory *mem = NULL;
  switch(header->mapper) {
    case NROM_MAPPER:
      mem = new Nrom(rom_file, header);
      break;
    case SXROM_MAPPER:
      mem = new Sxrom(rom_file, header);
      break;
    case UXROM_MAPPER:
      mem = new Uxrom(rom_file, header);
      break;
    default:
      fprintf(stderr, "Error: Rom requires unimplemented mapper: %d\n",
              static_cast<unsigned int>(header->mapper));
      delete header;
      return NULL;
      break;
  }

  return mem;
}

/*
 * Stores the provided header and allocates the palette array.
 */
Memory::Memory(RomHeader *header) {
  header_ = header;
  palette_data_ = new uint32_t[PALETTE_DATA_SIZE]();
  return;
}

/*
 * Reads an xRGB color from the palette.
 */
uint32_t Memory::PaletteRead(DoubleWord addr) {
  // Convert the address into an access to the palette data array.
  addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                         : (addr & PALETTE_BG_MASK);
  return palette_data_[addr] & PALETTE_XRGB_MASK;
}

/*
 * Uses the provided value to read a color from the palette; Then stores
 * the value and color in the palette data array using the provided address.
 *
 * Assumes that the palette has been initialized.
 */
void Memory::PaletteWrite(DoubleWord addr, DataWord val) {
  // Create the NES/xRGB pixel to be written.
  uint32_t pixel = (val << PALETTE_NES_PIXEL_SHIFT)
                 | ((ppu_->GetPalette())->Decode(val));

  // Convert the address into an access to the palette data array.
  addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                         : (addr & PALETTE_BG_MASK);
  palette_data_[addr] = pixel;

  return;
}

/*
 * Refreshes the xRGB pixels that are stored in the palette.
 *
 * Assumes that the palette has been initialized.
 */
void Memory::PaletteUpdate(void) {
  // Update each entry in the palette.
  DataWord nes_pixel;
  uint32_t pixel;
  for (size_t i = 0; i < PALETTE_DATA_SIZE; i++) {
    nes_pixel = palette_data_[i] >> PALETTE_NES_PIXEL_SHIFT;
    pixel = (nes_pixel << PALETTE_NES_PIXEL_SHIFT)
          | ((ppu_->GetPalette())->Decode(nes_pixel));
    palette_data_[i] = pixel;
  }
  return;
}

/*
 * Provides the memory object with access to its associated CPU, PPU, and APU
 * objects.
 *
 * This function must be called before the memory object can be used.
 */
void Memory::Connect(Cpu *cpu, Ppu *ppu, Apu *apu) {
  cpu_ = cpu;
  ppu_ = ppu;
  apu_ = apu;
  return;
}

/*
 * Uses the given input to create a controller object for this memory object.
 */
void Memory::AddController(Input *input) {
  // Remove the connected controller, if it exists.
  if (controller_ != NULL)  {
    delete controller_;
  }

  // Create the new controller.
  controller_ = new Controller(input);

  return;
}


/*
 * Frees the header and palatte data array associated with this memory class.
 */
Memory::~Memory(void) {
  delete header_;
  delete[] palette_data_;
  if (controller_ != NULL) { delete controller_; }
  return;
}
