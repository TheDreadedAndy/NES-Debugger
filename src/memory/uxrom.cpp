/*
 * Implementation of INES Mapper 2 (UxROM).
 *
 * The third quarter of addressable NES memory is mapped to a switchable
 * bank, which can be changed by writing to that section of memory.
 * The last quarter of memory is always mapped to the final bank.
 *
 * This implementation can address up to 256KB of cart memory.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./header.h"
#include "./uxrom.h"
#include "../util/data.h"
#include "../io/controller.h"

// Constants used to size and access memory.
#define BANK_SIZE 0x4000U
#define BANK_OFFSET 0x8000U
#define BANK_ADDR_MASK 0x3FFFU
#define UOROM_BANK_MASK 0x0FU
#define UNROM_BANK_MASK 0x07U
#define MAX_UNROM_BANKS 8
#define FIXED_BANK_OFFSET 0xC000U
#define BAT_SIZE 0x2000U
#define BAT_OFFSET 0x6000U
#define BAT_MASK 0x1FFFU
#define RAM_SIZE 0x800U
#define RAM_MASK 0x7FFU
#define PPU_OFFSET 0x2000U
#define IO_OFFSET 0x4000U
#define UXROM_OFFSET 0x4020U

// Constants used to size and access VRAM.
#define CHR_RAM_SIZE 0x2000U
#define NAMETABLE_SIZE 0x0400U
#define NAMETABLE_ACCESS_BIT 0x2000U
#define NAMETABLE_SELECT_MASK 0x0C00U
#define NAMETABLE_ADDR_MASK 0x03FFU
#define PATTERN_TABLE_MASK 0x1FFFU

/*
 * Uses the provided rom file and header to initialize the Uxrom class.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the provided header was created from the rom and is valid.
 */
Uxrom::Uxrom(FILE *rom_file, header_t *rom_header) {
  // Setup the ram space.
  ram = RandNew(RAM_SIZE);
  bat = RandNew(BAT_SIZE);

  // Load the rom data into memory.
  header = rom_header;
  LoadPrg(rom_file, rom_header);
  LoadChr(rom_file, rom_header);

  // Setup the nametable in vram.
  nametable[0] = RandNew(NAMETABLE_SIZE);
  nametable[3] = RandNew(NAMETABLE_SIZE);
  if (header->mirror) {
    // Vertical (horizontal arrangement) mirroring.
    nametable[1] = nametable[3];
    nametable[2] = nametable[0];
  } else {
    // Horizontal (vertical arrangement) mirroring.
    nametable[1] = nametable[0];
    nametable[2] = nametable[3];
  }

  return;
}

/*
 * Loads the program rom data into the calling Uxrom class during contruction.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the header field of the class is valid and matches the rom.
 */
void Uxrom::LoadPrg(FILE *rom_file) {
  // Calculate the number of prg banks used by the rom, then load
  // the rom into memory.
  size_t num_banks = (size_t) (header->prg_rom_size / BANK_SIZE);
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    cart[i] = new DataWord[BANK_SIZE];
    for (size_t j = 0; j < BANK_SIZE; j++) {
      cart[i][j] = fgetc(rom_file);
    }
  }

  // Setup the default banks and max banks.
  current_bank = 0;
  fixed_bank = num_banks - 1;
  bank_mask = (num_banks > MAX_UNROM_BANKS)
            ? UOROM_BANK_MASK : UNROM_BANK_MASK;

  return;
}

/*
 * Loads the CHR ROM data into the calling Uxrom class during construction.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the header field of the class is valid and matches the rom.
 */
void Uxrom::LoadChr(FILE *rom_file) {
  // Check if the rom is using chr-ram, and allocate it if so.
  if (header->chr_ram_size > 0) {
    is_chr_ram = true;
    pattern_table = RandNew(CHR_RAM_SIZE);
    return;
  }

  // Otherwise, the rom uses chr-rom and the data needs to be copied
  // from the rom file.
  is_chr_ram = false;
  pattern_table = new DataWord[header->chr_rom_size];
  fseek(rom_file, HEADER_SIZE + header->prg_rom_size, SEEK_SET);
  for (size_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
    pattern_table[i] = fgetc(rom_file);
  }

  return;
}

/*
 * Reads a value from the given address, accounting for MMIO and bank
 * switching. If the address given leads to an open bus, the last value
 * that was placed on the bus is returned instead.
 *
 * Assumes that Connect has been called on valid Cpu/Ppu/Apu classes for this
 * memory class.
 */
DataWord Uxrom::Read(DoubleWord addr) {
  // Detect where in memory we need to access and place the value on the bus.
  if (addr < PPU_OFFSET) {
    // Read from RAM.
    bus = ram[addr & RAM_MASK];
  } else if ((PPU_OFFSET <= addr) && (addr < IO_OFFSET)) {
    // Access PPU MMIO.
    bus = ppu->Read(addr);
  } else if ((IO_OFFSET <= addr) && (addr < UXROM_OFFSET)) {
    // Read from IO/APU MMIO.
    if ((addr == IO_JOY1_ADDR) || (addr == IO_JOY2_ADDR)) {
      bus = controller_read(addr);
    } else {
      bus = apu->Read(addr);
    }
  } else if ((BAT_OFFSET <= addr) && (addr < BANK_OFFSET)) {
    // Read from the roms WRAM/Battery.
    return M->bat[addr & BAT_MASK];
  } else if ((BANK_OFFSET <= addr) && (addr < FIXED_BANK_OFFSET)) {
    // Read from the switchable bank.
    return M->cart[M->current_bank][addr & BANK_ADDR_MASK];
  } else if (addr >= FIXED_BANK_OFFSET) {
    // Read from the fixed bank.
    return M->cart[M->fixed_bank][addr & BANK_ADDR_MASK];
  }

  return memory_bus;
}

/*
 * Writes a value to the given address, accounting for MMIO.
 *
 * Assumes that Connect has been called on valid Cpu/Ppu/Apu classes for this
 * memory class.
 */
void Uxrom::Write(DoubleWord addr, DataWord val) {
  // Put the value being written on the bus.
  bus = val;

  // Detect where in memory we need to access and do so.
  if (addr < PPU_OFFSET) {
    // Write to standard RAM.
    ram[addr & RAM_MASK] = val;
  } else if ((PPU_OFFSET <= addr) && (addr < IO_OFFSET)) {
    // Write to the MMIO for the PPU.
    ppu->Write(addr, val);
  } else if ((IO_OFFSET <= addr) && (addr < MAPPER_OFFSET)) {
    // Write to the general MMIO space.
    if (addr == CPU_DMA_ADDR) {
      cpu->StartDma(val);
    } else if (addr == IO_JOY1_ADDR) {
      ControllerWrite(addr, val);
    } else {
      apu->Write(addr, val);
    }
  } else if ((BAT_OFFSET <= addr) && (addr < BANK_OFFSET)) {
    // Write to the WRAM/Battery of the cart.
    M->bat[addr & BAT_MASK] = val;
  } else if (addr >= BANK_OFFSET) {
    // Writing to the cart area uses the low bits to select a bank.
    M->current_bank = val & M->bank_mask;
  }

  return;
}

/*
 * Reads a word from the remappable section of VRAM.
 *
 * Assumes the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
DataWord uxrom_vram_read(DoubleWord addr, void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Name table is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    return M->nametable[table][addr & NAMETABLE_ADDR_MASK];
  } else {
    // Pattern table is being accessed.
    return M->pattern_table[addr & PATTERN_TABLE_MASK];
  }
}

/*
 * Writes a word to the remappable section of VRAM.
 * Does nothing if the program attempts to write to CHR-ROM.
 *
 * Assumes the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
void uxrom_vram_write(DataWord val, DoubleWord addr, void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Name table is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    M->nametable[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if (M->is_chr_ram) {
    // Pattern table is being accessed.
    M->pattern_table[addr & PATTERN_TABLE_MASK] = val;
  }

  return;
}

/*
 * Takes in a uxrom memory structure and frees it.
 *
 * Assumes that the fixed bank is the final allocated bank.
 * Assumes the input pointer is a uxrom memory structure and is non-null.
 */
void uxrom_free(void *map) {
  // Cast back from the generic structure.
  uxrom_t *M = (uxrom_t*) map;

  // Free the contents of the structure.
  free(M->nametable[0]);
  free(M->nametable[3]);

  // Frees each bank.
  for (size_t i = 0; i < (M->fixed_bank + 1U); i++) {
    free(M->cart[i]);
  }

  // Free the structure itself.
  free(M);

  return;
}
