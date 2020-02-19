/*
 * Implementation of INES Mapper 0 (NROM).
 *
 * This is the most basic NES mapper, with no bank switching.
 *
 * This implementation supports up to 32KB of program data, 8KB of ram,
 * and 8KB of CHR data.
 */

#include "./nrom.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <new>

#include "../util/util.h"
#include "../util/data.h"
#include "../config/config.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../io/controller.h"
#include "./memory.h"
#include "./header.h"

// Constants used to size and access memory.
#define BANK_SIZE 0x4000U
#define BANK_OFFSET 0x8000U
#define BANK_OPT_OFFSET 0xC000U
#define BANK_ADDR_MASK 0x3FFFU
#define BAT_SIZE 0x2000U
#define BAT_OFFSET 0x6000U
#define BAT_MASK 0x1FFFU

// Constants used to size and access VRAM.
#define PATTERN_TABLE_SIZE 0x2000U
#define NAMETABLE_SIZE 0x0400U
#define NAMETABLE_SELECT_MASK 0x0C00U
#define NAMETABLE_ADDR_MASK 0x03FFU
#define PATTERN_TABLE_MASK 0x1FFFU

/*
 * Uses the provided header and rom file to create an Nrom object.
 *
 * Assumes the provided ROM file is non-null and points to a valid NES ROM.
 * Assumes the provided header is valid and was created using said ROM.
 */
Nrom::Nrom(FILE *rom_file, RomHeader *header, Config *config)
    : Memory(header, config) {
  // Setup cart and NES RAM.
  ram_ = RandNew(RAM_SIZE);
  bat_ = RandNew(BAT_SIZE);

  // Load the rom data into memory.
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
 * Loads the PRG-ROM section of a ROM into the mapped memory system.
 *
 * Assumes the provided ROM file is non-null and valid.
 * Assumes the calling object was provided with a valid ROM header.
 */
void Nrom::LoadPrg(FILE *rom_file) {
  // Calculate the number of PRG-ROM banks used by the ROM.
  size_t num_banks = static_cast<size_t>(header_->prg_rom_size / BANK_SIZE);

  // Throw an error if the rom file requested an invalid amount of space.
  if (num_banks > 2) {
    fprintf(stderr, "Error: The ROM file requested an invalid amount of "
                    "program memory for this mapper.\n");
    abort();
  }

  // Load the PRG-ROM into the Nrom object.
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_banks; i++) {
    cart_[i] = new DataWord[BANK_SIZE];
    for (size_t j = 0; j < BANK_SIZE; j++) {
      cart_[i][j] = fgetc(rom_file);
    }
  }

  // If the cart contains only 16KB of PRG-ROM, we mirror it into
  // the second bank.
  if (num_banks < 2) { cart_[1] = cart_[0]; }

  return;
}

/*
 * Loads the CHR section of a ROM into the mapped memory system.
 *
 * Assumes the provided ROM file is valid.
 * Assumes the calling object was provided with a valid header for the
 * given ROM.
 */
void Nrom::LoadChr(FILE *rom_file) {
  // Check if the rom is using chr-ram, and allocate it if so.
  if (header_->chr_ram_size > 0) {
    is_chr_ram_ = true;
    pattern_table_ = RandNew(header_->chr_ram_size);
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
 * Reads a value from the given address, accounting for MMIO and
 * open bus behavior.
 *
 * Assumes that Connect has been called by the calling object on valid
 * Cpu/Ppu/Apu objects.
 * Assumes that AddController has been called by the calling object on
 * a valid Input object.
 */
DataWord Nrom::Read(DoubleWord addr) {
  // Detect where in memory we need to access and do so.
  if (addr < PPU_OFFSET) {
    // Read from system RAM.
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
    // Read from the carts working RAM.
    bus_ = bat_[addr & BAT_MASK];
  } else if ((BANK_OFFSET <= addr) && (addr < BANK_OPT_OFFSET)) {
    // Read from the low bank.
    bus_ = cart_[0][addr & BANK_ADDR_MASK];
  } else if (addr >= BANK_OPT_OFFSET) {
    // Read from the high bank.
    bus_ = cart_[1][addr & BANK_ADDR_MASK];
  }

  return bus_;
}

/*
 * Attempts to write the given value to the given address, accounting for MMIO.
 *
 * Assumes that Connect has been called by the calling object on valid
 * Cpu/Ppu/Apu objects.
 * Assumes that AddController has been called by the calling object on
 * a valid Input object.
 */
void Nrom::Write(DoubleWord addr, DataWord val) {
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
  }

  return;
}

/*
 * Checks if a read to the given address will have side effects outside
 * the CPU. Returns true if the operation is safe, and false if there
 * are side effects.
 */
bool Nrom::CheckRead(DoubleWord addr) {
  return (addr < PPU_OFFSET) || (addr >= MAPPER_OFFSET);
}

/*
 * Checks if a write to the given address will have side effects outside
 * the CPU. Returns true if the operation is safe, and false if there
 * are side effects.
 */
bool Nrom::CheckWrite(DoubleWord addr) {
  return (addr < PPU_OFFSET) || (addr >= MAPPER_OFFSET);
}

/*
 * Reads the value at the given address in VRAM.
 */
DataWord Nrom::VramRead(DoubleWord addr) {
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
 * Attempts to write the given value to VRAM.
 * Does nothing if the program attempts to write to CHR-ROM.
 * Updates the palette data array if the palette is written to.
 */
void Nrom::VramWrite(DoubleWord addr, DataWord val) {
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
 * Deletes the calling Nrom object.
 */
Nrom::~Nrom(void) {
  // Delete the NES system memory.
  delete[] ram_;

  // Delete the cart's memory.
  delete[] bat_;
  delete[] cart_[0];
  if (cart_[0] != cart_[1]) { delete[] cart_[1]; }

  // Delete the PPU's ram space.
  delete[] pattern_table_;
  delete[] nametable_[0];
  delete[] nametable_[3];

  return;
}
