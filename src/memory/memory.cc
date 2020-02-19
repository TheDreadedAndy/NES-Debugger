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
#include "../cpu/cpu.h"
#include "../io/controller.h"
#include "../apu/apu.h"
#include "./header.h"
#include "./palette.h"
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
Memory *Memory::Create(FILE *rom_file, Config *config) {
  // Use the provided rom file to create a decoded rom header.
  RomHeader *header = DecodeHeader(rom_file);
  if (header == NULL) { return NULL; }

  // Use the decoded header to decide which memory structure should be created.
  Memory *mem = NULL;
  switch(header->mapper) {
    case NROM_MAPPER:
      mem = new Nrom(rom_file, header, config);
      break;
    case SXROM_MAPPER:
      mem = new Sxrom(rom_file, header, config);
      break;
    case UXROM_MAPPER:
      mem = new Uxrom(rom_file, header, config);
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
Memory::Memory(RomHeader *header, Config *config) {
  // Load in the header and setup the palette data arrays.
  header_ = header;
  palette_data_ = new DataWord[PALETTE_DATA_SIZE]();
  pixel_data_ = new Pixel[PALETTE_DATA_SIZE]();

  // Create a palette structure using the given configuration.
  palette_ = new NesPalette(config->Get(kPaletteFileKey));

  return;
}

/*
 * Reads an NES pixel from palette data.
 */
DataWord Memory::PaletteRead(DoubleWord addr) {
  // Convert the address into an access to the palette data array.
  return palette_data_[addr & PALETTE_ADDR_MASK];
}

/*
 * Writes the given value to the palette data array, then decodes the value
 * and writes it to the pixel data array. If the value is at a mirrored
 * address, then it is written again to the mirrored position in both arrays.
 *
 * Assumes that the palette has been initialized.
 */
void Memory::PaletteWrite(DoubleWord addr, DataWord val) {
  // Used to check if a palette access needs to be mirrored.
  const DoubleWord kPaletteMirrorAccessMask = 0x03U;
  const DoubleWord kPaletteMirrorBit = 0x10U;

  // Update the palette and pixel arrays.
  addr &= PALETTE_ADDR_MASK;
  Pixel pixel_val = palette_->Decode(val);
  palette_data_[addr] = val;
  pixel_data_[addr] = pixel_val;

  // Check if the address is mirrored, and update its mirror if it is.
  // Values whose low 2 bits are zero are mirrored.
  if ((addr & kPaletteMirrorAccessMask) == 0) {
    addr ^= kPaletteMirrorBit;
    palette_data_[addr] = val;
    pixel_data_[addr] = pixel_val;
  }

  return;
}

/*
 * Updates the mask selection of the palette, then refreshes the pixel data.
 *
 * Assumes the palette has been initialized.
 */
void Memory::PaletteUpdate(DataWord mask) {
  // Updates the palette mask.
  palette_->UpdateMask(mask);

  // Update each entry in the pixel array.
  for (size_t i = 0; i < PALETTE_DATA_SIZE; i++) {
    pixel_data_[i] = palette_->Decode(palette_data_[i]);
  }

  return;
}

/*
 * Exposes the pixel data to the caller. The exposed data must not be modified.
 *
 * Assumes the palette has been initialized.
 */
const Pixel *Memory::PaletteExpose(void) {
  return const_cast<const Pixel*>(pixel_data_);
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
 * Frees the structures and objects associated with this class.
 */
Memory::~Memory(void) {
  delete header_;
  delete[] palette_data_;
  delete[] pixel_data_;
  delete palette_;
  if (controller_ != NULL) { delete controller_; }
  return;
}
