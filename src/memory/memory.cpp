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

// RAM data constants.
#define RAM_SIZE 0x800U
#define RAM_MASK 0x7FFU

// The address at which PPU registers are located.
#define PPU_OFFSET 0x2000U

// The address at which APU and IO registers are located.
#define IO_OFFSET 0x4000U

// The address at which memory begins  to depend on the mapper in use.
#define MAPPER_OFFSET 0x4020U

// Size of NES palette data.
#define PALETTE_SIZE 0x20U

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
Memory *MemoryCreate(FILE *rom_file, header_t *rom_header) {
  // Use the decoded header to decide which memory structure should be created.
  Memory *mem = NULL;
  switch(header->mapper) {
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
              static_cast<unsigned int>(header->mapper));
      break;
  }

  return mem;
}

/*
 * This must be here to prevent issues with declaring a pure
 * virtual constructor.
 */
Memory::Memory(void) {
  return;
}

//TODO: Remove all below this line.

/*
 * Uses the generic memory structures read function to read a word
 * from memory.
 *
 * Assumes the memory structure is valid.
 */
DataWord memory_read(DoubleWord addr) {
  // Determine if the NES address space or the mapper should be accessed.
  if (addr < PPU_OFFSET) {
    // Access standard ram.
    memory_bus = system_memory->ram[addr & RAM_MASK];
  } else if (addr < IO_OFFSET) {
    // Access PPU MMIO.
    memory_bus = ppu_read(addr);
  } else if (addr < MAPPER_OFFSET) {
    // Access APU and IO registers.
    if ((addr == IO_JOY1_ADDR) || (addr == IO_JOY2_ADDR)) {
      // Read from controller register.
      memory_bus = controller_read(addr);
    } else {
      // Read from APU register.
      memory_bus = apu_read(addr);
    }
  } else {
    // Access cartridge space using the mapper.
    memory_bus = system_memory->read(addr, system_memory->map);
  }

  // Read the value from the bus.
  return memory_bus;
}

/*
 * Uses the generic memory structures write function to write a word
 * to memory.
 *
 * Assumes the memory structure is valid.
 */
void memory_write(DataWord val, DoubleWord addr) {
  // Put the value being written on the bus.
  memory_bus = val;

  // Determine if the NES address space or the mapper should be accessed.
  if (addr < PPU_OFFSET) {
    // Access standard ram.
    system_memory->ram[addr & RAM_MASK] = val;
  } else if (addr < IO_OFFSET) {
    // Access PPU MMIO.
    ppu_write(addr, val);
  } else if (addr < MAPPER_OFFSET) {
    // Access APU and IO registers.
    if (addr == CPU_DMA_ADDR) {
      cpu_start_dma(val);
    } else if (addr == IO_JOY1_ADDR) {
      controller_write(val, addr);
    } else {
      apu_write(addr, val);
    }
  } else {
    system_memory->write(val, addr, system_memory->map);
  }

  return;
}

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

/*
 * Reads an xRGB color from the palette.
 *
 * Assumes memory has been initialized.
 */
uint32_t memory_palette_read(DoubleWord addr) {
  // Convert the address into an access to the palette data array.
  addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                         : (addr & PALETTE_BG_MASK);
  return system_memory->palette_data[addr] & PALETTE_XRGB_MASK;
}

/*
 * Refreshes the xRGB pixels that are stored in the palette.
 *
 * Assumes memory has been initialized.
 * Assumes the palette has been initialized.
 */
void memory_palette_update(void) {
  // Update each entry in the palette.
  DataWord nes_pixel;
  uint32_t pixel;
  for (size_t i = 0; i < PALETTE_SIZE; i++) {
    nes_pixel = system_memory->palette_data[i] >> PALETTE_NES_PIXEL_SHIFT;
    pixel = (nes_pixel << PALETTE_NES_PIXEL_SHIFT) | palette_decode(nes_pixel);
    system_memory->palette_data[i] = pixel;
  }
  return;
}

/*
 * Frees the generic memory structure.
 * Assumes that the structure is valid.
 */
void memory_free(void) {
  // Free the mapper structure using its specified function.
  system_memory->free(system_memory->map);

  // Free the generic structure.
  free(system_memory->header);
  free(system_memory->ram);
  free(system_memory->palette_data);
  free(system_memory);
}
