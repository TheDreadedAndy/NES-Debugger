/*
 * NES memory implementation.
 *
 * Abstracts away memory mapping from the 2A03.
 *
 * TODO
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./header.h"
#include "./nrom.h"
#include "./sxrom.h"
#include "./uxrom.h"
#include "../util/data.h"
#include "../ppu/ppu.h"
#include "../ppu/palette.h"
#include "../cpu/2A03.h"
#include "../io/controller.h"
#include "../apu/apu.h"

// Size of NES palette data.
#define PALETTE_DATA_SIZE 0x20U

// Used to determine if (and where) the palette is being addressed.
#define PALETTE_ACCESS_MASK 0x3F00U
#define PALETTE_ADDR_MASK 0x001FU
#define PALETTE_BG_ACCESS_MASK 0x0003U
#define PALETTE_BG_MASK 0x000CU

// Used to interact with the optimized palette.
#define PALETTE_NES_PIXEL_SHIFT 24U
#define PALETTE_XRGB_MASK 0x00FFFFFFU

// VRAM can only address 14 bits.
#define VRAM_ADDR_MASK 0x3FFFU

/*
 * Uses the header to determine which memory mapper should be created, and
 * creates it. The mapper is filled with the data from the provided rom file.
 *
 * On success, the returned value is the memory mapper class cast to Memory.
 * On failure, returns NULL.
 *
 * Assumes the provided header is non-null and valid.
 * Assumes the provided rom file is non-null and a valid NES rom.
 * Assumes that the provided header was generated using the given rom file.
 */
Memory *MemoryCreate(FILE *rom_file) {
  // Use the provided rom file to create a decoded rom header.
  RomHeader *rom_header = DecodeHeader(rom_file);
  if (rom_header == NULL) { return NULL; }

  // Use the decoded header to decide which memory structure should be created.
  Memory *mem = NULL;
  switch(rom_header->mapper) {
    case NROM_MAPPER:
      mem = new Nrom(rom_file, rom_header);
      break;
    case SXROM_MAPPER:
      mem = new Sxrom(rom_file, rom_header);
      break;
    case UXROM_MAPPER:
      mem = new Uxrom(rom_file, rom_header);
      break;
    default:
      fprintf(stderr, "Error: Rom requires unimplemented mapper: %d\n",
              static_cast<unsigned int>(rom_header->mapper));
      break;
  }

  return mem;
}

/*
 * Reads an xRGB color from the palette.
 */
uint32_t Memory::PaletteRead(DoubleWord addr) {
  // Convert the address into an access to the palette data array.
  addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                         : (addr & PALETTE_BG_MASK);
  return palette_data[addr] & PALETTE_XRGB_MASK;
}

/*
 * TODO
 */
void Memory::PaletteWrite(DoubleWord addr, DataWord val) {
  return;
}

/*
 * Refreshes the xRGB pixels that are stored in the palette.
 */
void Memory::PaletteUpdate(void) {
  // Update each entry in the palette.
  DataWord nes_pixel;
  uint32_t pixel;
  for (size_t i = 0; i < PALETTE_SIZE; i++) {
    nes_pixel = palette_data[i] >> PALETTE_NES_PIXEL_SHIFT;
    pixel = (nes_pixel << PALETTE_NES_PIXEL_SHIFT) | palette_decode(nes_pixel);
    palette_data[i] = pixel;
  }
  return;
}

/*
 * Stores the provided header and allocates the palette array.
 */
Memory::Memory(RomHeader *rom_header) {
  header = rom_header;
  palette_data = new uint32_t[PALETTE_DATA_SIZE];
  return;
}

/*
 * Frees the header and palatte data array associated with this memory class.
 */
Memory::~Memory() {
  delete header;
  delete[] palette_data;
  return;
}

//TODO: Remove all below this line.

/*
 * Uses the generic memory structures vram read function to read
 * a word from vram. Handles palette accesses.
 *
 * Assumes memory has been initialized.
 */
DataWord memory_vram_read(DoubleWord addr) {
  // Mask out any extra bits.
  addr &= VRAM_ADDR_MASK;

  // Check if the palette is being accessed.
  if ((addr & PALETTE_ACCESS_MASK) == PALETTE_ACCESS_MASK) {
    // Convert the address into an access to the palette data array.
    addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                           : (addr & PALETTE_BG_MASK);
    return system_memory->palette_data[addr] >> PALETTE_NES_PIXEL_SHIFT;
  } else {
    // Access the pattern table/nametable.
    return system_memory->vram_read(addr, system_memory->map);
  }
}

/*
 * Uses the generic memory structures vram write function to write
 * a word to vram. Handles palette accesses.
 *
 * Assumes memory has been initialized.
 * Assumes the palette has been initialized.
 */
void memory_vram_write(DataWord val, DoubleWord addr) {
  // Mask out any extra bits.
  addr &= VRAM_ADDR_MASK;

  // Check if the palette is being accessed.
  if ((addr & PALETTE_ACCESS_MASK) == PALETTE_ACCESS_MASK) {
    // Create the NES/xRGB pixel to be written.
    uint32_t pixel = (val << PALETTE_NES_PIXEL_SHIFT) | palette_decode(val);

    // Convert the address into an access to the palette data array.
    addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                           : (addr & PALETTE_BG_MASK);
    system_memory->palette_data[addr] = pixel;
  } else {
    // Access the pattern table/nametable.
    system_memory->vram_write(val, addr, system_memory->map);
  }

  return;
}
