/*
 * Implementation of INES Mapper 2 (UxROM).
 *
 * The third quarter of addressable NES memory is mapped to a switchable
 * bank, which can be changed by writing to that section of memory.
 * The last quarter of memory is always mapped to the final bank.
 *
 * This implementation can address up to 256KB of cart memory.
 */

#include "./std_banked.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../../util/util.h"
#include "../../util/data.h"
#include "../../config/config.h"
#include "../../io/controller.h"
#include "../../cpu/cpu.h"
#include "../../ppu/ppu.h"
#include "../../apu/apu.h"
#include "../memory.h"
#include "../header.h"

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
 * Uses the provided rom file and header to initialize the StdBanked class.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the provided header was created from the rom and is valid.
 */
StdBanked::StdBanked(FILE *rom_file, RomHeader *header, Config *config)
         : Memory(header, config) {
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
 * Loads the program rom data into the calling StdBanked class during contruction.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the header field of the class is valid and matches the rom.
 */
void StdBanked::LoadPrg(FILE *rom_file) {
  // Calculate and verify the number of PRG-ROM banks.
  size_t num_banks = static_cast<size_t>(header_->prg_rom_size / BANK_SIZE);
  if ((header_->mapper == NROM) && (num_banks > 2)) {
    fprintf(stderr, "Error: The ROM file requested an invalid amount of "
                    "program memory for its mapper.\n");
    abort();
  }

  // Load the rom into memory.
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    cart_[i] = new DataWord[BANK_SIZE];
    for (size_t j = 0; j < BANK_SIZE; j++) {
      cart_[i][j] = fgetc(rom_file);
    }
  }

  // Setup the default banks and max banks.
  fixed_bank_ = num_banks - 1;
  bank_mask_ = 0;
  if (header_->mapper == UXROM) {
    bank_mask_ = (num_banks > MAX_UNROM_BANKS)
               ? UOROM_BANK_MASK : UNROM_BANK_MASK;
  }

  return;
}

/*
 * Loads the CHR ROM data into the calling StdBanked class during construction.
 *
 * Assumes the provided rom file is non-null and points to a valid NES rom.
 * Assumes the header field of the class is valid and matches the rom.
 */
void StdBanked::LoadChr(FILE *rom_file) {
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
 * Assumes that AddController has been called by the calling object on
 * a valid Input object.
 */
DataWord StdBanked::Read(DoubleWord addr) {
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
    bus_ = bat_[addr & BAT_MASK];
  } else if ((BANK_OFFSET <= addr) && (addr < FIXED_BANK_OFFSET)) {
    // Read from the switchable bank.
    bus_ = cart_[current_bank_][addr & BANK_ADDR_MASK];
  } else if (addr >= FIXED_BANK_OFFSET) {
    // Read from the fixed bank.
    bus_ = cart_[fixed_bank_][addr & BANK_ADDR_MASK];
  }

  return bus_;
}

/*
 * TODO
 */
DataWord StdBanked::Inspect(DoubleWord addr, int sel) {
  (void)addr;
  (void)sel;
  return 0;
}

/*
 * Writes a value to the given address, accounting for MMIO.
 *
 * Assumes that Connect has been called on valid Cpu/Ppu/Apu classes for this
 * memory class.
 */
void StdBanked::Write(DoubleWord addr, DataWord val) {
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
  } else if ((addr >= BANK_OFFSET) && bank_mask_) {
    // Writing to the cart area uses the low bits to select a bank.
    // This is disabled when the bank mask is 0.
    current_bank_ = val & bank_mask_;
  }

  return;
}

/*
 * Checks if the given address can be read from without side effects outside
 * the CPU.
 */
bool StdBanked::CheckRead(DoubleWord addr) {
  return (addr < PPU_OFFSET) || (addr >= MAPPER_OFFSET);
}

/*
 * Checks if the given address can be written to without side effects outside
 * the CPU.
 */
bool StdBanked::CheckWrite(DoubleWord addr) {
  return (addr < PPU_OFFSET) || ((addr >= MAPPER_OFFSET)
      && ((addr < BANK_OFFSET) || !bank_mask_));
}

/*
 * Reads the value at the given address from vram, accounting
 * for mirroring.
 */
DataWord StdBanked::VramRead(DoubleWord addr) {
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
    // Palette is being accessed.
    return PaletteRead(addr);
  }
}

/*
 * Writes the given value to vram, accounting for mirroring.
 * Update the palette data array if a value is written to the palette.
 * Does nothing if the program attempts to write to CHR-ROM.
 */
void StdBanked::VramWrite(DoubleWord addr, DataWord val) {
  // Masks out any extra bits.
  addr &= VRAM_BUS_MASK;

  // Determine which part of VRAM is being accessed.
  if ((addr < NAMETABLE_OFFSET) && is_chr_ram_) {
    pattern_table_[addr & PATTERN_TABLE_MASK] = val;
  } else if ((NAMETABLE_OFFSET <= addr) && (addr < PALETTE_OFFSET)) {
    // Name table is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> 10;
    nametable_[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if (addr >= PALETTE_OFFSET) {
    // Palette data is being accessed.
    PaletteWrite(addr, val);
  }

  return;
}

/*
 * Deletes the calling StdBanked object.
 *
 * Assumes that the fixed bank is the final allocated bank.
 */
StdBanked::~StdBanked(void) {
  // Free the CPU memory arrays.
  delete[] ram_;
  delete[] bat_;

  // Free the VRAM data.
  delete[] nametable_[0];
  delete[] nametable_[3];
  delete[] pattern_table_;

  // Frees each bank of CPU memory.
  for (size_t i = 0; i < (fixed_bank_ + 1U); i++) {
    delete[] cart_[i];
  }

  return;
}
