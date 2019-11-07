/*
 * Implementation of INES Mapper 2 (UxROM).
 *
 * The third quarter of addressable NES memory is mapped to a switchable
 * bank, which can be changed by writing to that section of memory.
 * The last quarter of memory is always mapped to the final bank.
 *
 * This implementation can address up to 256KB of cart memory.
 */

#include "./uxrom.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/util.h"
#include "../util/data.h"
#include "../io/controller.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "./memory.h"
#include "./header.h"

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

// Constants used to size and access VRAM.
#define CHR_RAM_SIZE 0x2000U
#define PATTERN_TABLE_SIZE 0x2000U
#define PATTERN_TABLE_MASK 0x1FFFU

/*
 * Uses the provided rom file and header to initialize the Uxrom class.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the provided header was created from the rom and is valid.
 */
Uxrom::Uxrom(FILE *rom_file, RomHeader *header) : Memory(header) {
  // Setup the ram space.
  ram_ = RandNew(RAM_SIZE);
  bat_ = RandNew(BAT_SIZE);

  // Load the rom data into memory.
  header_ = header;
  LoadPrg(rom_file);
  LoadChr(rom_file);

  // Setup the nametable in vram.
  nametable_[0] = RandNew(NAMETABLE_SIZE);
  nametable_[3] = RandNew(NAMETABLE_SIZE);
  if (header_->mirror) {
    // Vertical (horizontal arrangement) mirroring.
    nametable_[1] = nametable_[3];
    nametable_[2] = nametable_[0];
  } else {
    // Horizontal (vertical arrangement) mirroring.
    nametable_[1] = nametable_[0];
    nametable_[2] = nametable_[3];
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
  size_t num_banks = (size_t) (header_->prg_rom_size / BANK_SIZE);
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    cart_[i] = new DataWord[BANK_SIZE];
    for (size_t j = 0; j < BANK_SIZE; j++) {
      cart_[i][j] = fgetc(rom_file);
    }
  }

  // Setup the default banks and max banks.
  current_bank_ = 0;
  fixed_bank_ = num_banks - 1;
  bank_mask_ = (num_banks > MAX_UNROM_BANKS)
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
  if (header_->chr_ram_size > 0) {
    is_chr_ram_ = true;
    pattern_table_ = RandNew(CHR_RAM_SIZE);
    return;
  }

  // Otherwise, the rom uses chr-rom and the data needs to be copied
  // from the rom file.
  is_chr_ram_ = false;
  pattern_table_ = new DataWord[header_->chr_rom_size];
  fseek(rom_file, HEADER_SIZE + header_->prg_rom_size, SEEK_SET);
  for (size_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
    pattern_table_[i] = fgetc(rom_file);
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
    bus_ = ram_[addr & RAM_MASK];
  } else if ((PPU_OFFSET <= addr) && (addr < IO_OFFSET)) {
    // Access PPU MMIO.
    bus_ = ppu_->Read(addr);
  } else if ((IO_OFFSET <= addr) && (addr < MAPPER_OFFSET)) {
    // Read from IO/APU MMIO.
    if ((addr == IO_JOY1_ADDR) || (addr == IO_JOY2_ADDR)) {
      bus_ = controller_->Read(addr);
    } else {
      bus_ = apu_->Read(addr);
    }
  } else if ((BAT_OFFSET <= addr) && (addr < BANK_OFFSET)) {
    // Read from the roms WRAM/Battery.
    return bat_[addr & BAT_MASK];
  } else if ((BANK_OFFSET <= addr) && (addr < FIXED_BANK_OFFSET)) {
    // Read from the switchable bank.
    return cart_[current_bank_][addr & BANK_ADDR_MASK];
  } else if (addr >= FIXED_BANK_OFFSET) {
    // Read from the fixed bank.
    return cart_[fixed_bank_][addr & BANK_ADDR_MASK];
  }

  return bus_;
}

/*
 * Writes a value to the given address, accounting for MMIO.
 *
 * Assumes that Connect has been called on valid Cpu/Ppu/Apu classes for this
 * memory class.
 */
void Uxrom::Write(DoubleWord addr, DataWord val) {
  // Put the value being written on the bus.
  bus_ = val;

  // Detect where in memory we need to access and do so.
  if (addr < PPU_OFFSET) {
    // Write to standard RAM.
    ram_[addr & RAM_MASK] = val;
  } else if ((PPU_OFFSET <= addr) && (addr < IO_OFFSET)) {
    // Write to the MMIO for the PPU.
    ppu_->Write(addr, val);
  } else if ((IO_OFFSET <= addr) && (addr < MAPPER_OFFSET)) {
    // Write to the general MMIO space.
    if (addr == CPU_DMA_ADDR) {
      cpu_->StartDma(val);
    } else if (addr == IO_JOY1_ADDR) {
      controller_->Write(addr, val);
    } else {
      apu_->Write(addr, val);
    }
  } else if ((BAT_OFFSET <= addr) && (addr < BANK_OFFSET)) {
    // Write to the WRAM/Battery of the cart.
    bat_[addr & BAT_MASK] = val;
  } else if (addr >= BANK_OFFSET) {
    // Writing to the cart area uses the low bits to select a bank.
    current_bank_ = val & bank_mask_;
  }

  return;
}

/*
 * Reads the value at the given address from vram, accounting
 * for mirroring.
 */
DataWord Uxrom::VramRead(DoubleWord addr) {
  // Mask out any extra bits.
  addr &= VRAM_BUS_MASK;

  // Determine which part of VRAM is being accessed.
  if (addr < NAMETABLE_OFFSET) {
    // Pattern table is being accessed.
    return pattern_table_[addr & PATTERN_TABLE_MASK];
  } else if (addr < PALETTE_OFFSET) {
    // Nametable is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> 10U;
    return nametable_[table][addr & NAMETABLE_ADDR_MASK];
  } else {
    // Convert the address into an access to the palette data array.
    addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                           : (addr & PALETTE_BG_MASK);
    return palette_data_[addr] >> PALETTE_NES_PIXEL_SHIFT;
  }
}

/*
 * Writes the given value to vram, accounting for mirroring.
 * Update the palette data array if a value is written to the palette.
 * Does nothing if the program attempts to write to CHR-ROM.
 */
void Uxrom::VramWrite(DoubleWord addr, DataWord val) {
  // Masks out any extra bits.
  addr &= VRAM_BUS_MASK;

  // Determine which part of VRAM is being accessed.
  if ((addr < NAMETABLE_OFFSET) && is_chr_ram_) {
    pattern_table_[addr & PATTERN_TABLE_MASK] = val;
  } else if ((NAMETABLE_OFFSET <= addr) && (addr < PALETTE_OFFSET)) {
    // Name table is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    nametable_[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if (addr > PALETTE_OFFSET) {
    // Palette data is being accessed.
    PaletteWrite(addr, val);
  }

  return;
}

/*
 * Takes in a uxrom memory structure and frees it.
 *
 * Assumes that the fixed bank is the final allocated bank.
 * Assumes the input pointer is a uxrom memory structure and is non-null.
 */
Uxrom::~Uxrom(void) {
  // Free the CPU memory arrays.
  delete ram_;
  delete bat_;

  // Free the VRAM data.
  delete nametable_[0];
  delete nametable_[3];
  delete pattern_table_;

  // Frees each bank of CPU memory.
  for (size_t i = 0; i < (fixed_bank_ + 1U); i++) {
    delete cart_[i];
  }

  return;
}
