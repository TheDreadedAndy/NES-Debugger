/*
 * Implementation of INES Mapper 1 (SxROM).
 *
 * This mapper is interfaced using a serial connection to several controlling
 * registers. The original INES standard does not define how large each section
 * of memory should be, and so this implementation makes an educated guess
 * for roms using that standard.
 *
 * SxROM features control over nametable mirroring, bank switched CHR memory,
 * bank switched PRG-ROM, and bank switched PRG-RAM.
 */

#include "./sxrom.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/util.h"
#include "../util/data.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../io/controller.h"
#include "./memory.h"
#include "./header.h"

// Constants used to size and access memory.
#define ROM_BANK_SIZE 0x4000U
#define RAM_BANK_SIZE 0x2000U
#define PRG_RAM_OFFSET 0x6000U
#define PRG_ROM_A_OFFSET 0x8000U
#define PRG_ROM_B_OFFSET 0xC000U
#define PRG_RAM_MASK 0x1FFFU
#define PRG_ROM_MASK 0x3FFFU

// Constants used to control accesses to memory.
#define FLAG_CONTROL_RESET 0x80U
#define CONTROL_RESET_MASK 0x0CU
#define CONTROL_UPDATE_OFFSET 0x8000U
#define CHR_A_UPDATE_OFFSET 0xA000U
#define CHR_B_UPDATE_OFFSET 0xC000U
#define PRG_UPDATE_OFFSET 0xE000U
#define NAMETABLE_CONTROL_MASK 0x03U
#define NAMETABLE_MIRROR_LOW 0U
#define NAMETABLE_MIRROR_HIGH 1U
#define NAMETABLE_MIRROR_VERT 2U
#define NAMETABLE_MIRROR_HORI 3U
#define PRG_ROM_CONTROL_MASK 0x0CU
#define PRG_ROM_MODE_32K 0x00U
#define PRG_ROM_MODE_32K_ALT 0x04U
#define PRG_ROM_MODE_FIX_LOW 0x08U
#define PRG_ROM_MODE_FIX_HIGH 0x0CU
#define FLAG_CHR_MODE 0x10U
#define PRG_ROM_BANK_LOW_MASK 0x0FU
#define FLAG_PRG_RAM_DISABLE 0x10U

// Constants used to size and access VRAM.
#define CHR_BANK_SIZE 0x1000U
#define SCREEN_SIZE 0x400U
#define NAMETABLE_ACCESS_BIT 0x2000U
#define NAMETABLE_SELECT_MASK 0x0C00U
#define NAMETABLE_SELECT_SHIFT 10U
#define NAMETABLE_ADDR_MASK 0x03FFU
#define PATTERN_TABLE_HIGH_ACCESS_BIT 0x1000U
#define PATTERN_TABLE_MASK 0x0FFFU

/*
 * Uses the provided rom file and header to initialize and create an
 * Sxrom class.
 *
 * Assumes the provided rom file and header are valid.
 */
Sxrom::Sxrom(FILE *rom_file, RomHeader *header) : Memory(header) {
  // Setup the NES ram space.
  ram_ = RandNew(RAM_SIZE);

  // Load the rom data into memory.
  LoadPrgRom(rom_file);
  LoadChr(rom_file);

  // Set up the cart ram space.
  LoadPrgRam();

  // Setup the nametable in vram. The header mirroring bit is ignored in
  // this mapper, so we set the mirrored banks to a default value for now.
  nametable_bank_a_ = RandNew(SCREEN_SIZE);
  nametable_bank_b_ = RandNew(SCREEN_SIZE);
  nametable_[0] = nametable_bank_a_;
  nametable_[1] = nametable_bank_a_;
  nametable_[2] = nametable_bank_a_;
  nametable_[3] = nametable_bank_a_;

  return;
}

/*
 * Loads the given rom file into the sxrom memory object.
 * Note that while this implementation assumes the banks will boot in mode 3,
 * with the upper half of rom fixed to the last bank and the lower half on bank
 * 0, this is not specified in original hardware.
 *
 * Assumes the provided rom_file is valid.
 * Assumes the object was initialized with a valid header structure.
 */
void Sxrom::LoadPrgRom(FILE *rom_file) {
  // Get the number of PRG-ROM banks, then load the rom into memory.
  num_prg_rom_banks_ = header_->prg_rom_size / ROM_BANK_SIZE;
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < num_prg_rom_banks_; i++) {
    prg_rom_[i] = new DataWord[ROM_BANK_SIZE];
    for (size_t j = 0; j < ROM_BANK_SIZE; j++) {
      prg_rom_[i][j] = fgetc(rom_file);
    }
  }

  // Determine if the requested PRG-ROM size needs a fifth selection bit.
  if (num_prg_rom_banks_ > 16) {
    prg_rom_high_mask_ = 0x10;
  } else {
    prg_rom_high_mask_ = 0;
  }

  // Setup the default bank mode.
  prg_rom_bank_b_ = num_prg_rom_banks_ - 1;

  return;
}

/*
 * Determines if the given rom is using CHR-ROM or CHR-RAM, then creates
 * the necessary banks. Loads in the CHR data if the board is using CHR-ROM.
 * Creates a mask for accessing the CHR control register.
 *
 * This function aborts if the created mask conflicts with the PRG-ROM mask.
 *
 * Assumes the provided rom file is valid.
 * Assumes that the calling object has had its PRG-ROM initialized.
 */
void Sxrom::LoadChr(FILE *rom_file) {
  // Check if the rom is using CHR-RAM, and allocate it if so.
  if (header_->chr_ram_size > 0) {
    is_chr_ram_ = true;
    num_chr_banks_ = header_->chr_ram_size / CHR_BANK_SIZE;
    for (size_t i = 0; i < num_chr_banks_; i++) {
      pattern_table_[i] = RandNew(CHR_BANK_SIZE);
    }
  } else {
    // Otherwise, the rom is using CHR-ROM and we must load it into the mapper.
    is_chr_ram_ = false;
    num_chr_banks_ = header_->chr_rom_size / CHR_BANK_SIZE;
    fseek(rom_file, HEADER_SIZE + header_->prg_rom_size, SEEK_SET);
    for (size_t i = 0; i < num_chr_banks_; i++) {
      pattern_table_[i] = new DataWord[CHR_BANK_SIZE];
      for (size_t j = 0; j < CHR_BANK_SIZE; j++) {
        pattern_table_[i][j] = fgetc(rom_file);
      }
    }
  }

  // Calculate the mask to be used to select the CHR banks in the CHR control
  // register.
  chr_bank_mask_ = CreateMask(num_chr_banks_);

  // Check if the requested rom size conflicts with the requested number of
  // CHR banks.
  if (chr_bank_mask_ & prg_rom_high_mask_) {
    fprintf(stderr, "Error: The requested amount of PRG-ROM cannot be "
                    "addressed with the given CHR size.\n");
    abort();
  }

  return;
}

/*
 * Creates an 8-bit bitmask that can select the provided number of elements.
 */
DataWord Sxrom::CreateMask(DataWord items) {
  DataWord msb = MsbWord(items);
  if ((msb != 0) && (msb == items)) {
    return msb - 1U;
  } else if (msb != 0) {
    return (msb << 1U) - 1U;
  } else {
    return 0U;
  }
}

/*
 * Creates a ram space with the amount of memory requested by the mapper.
 * If an INES header is used, the size is not defined and is created
 * based on the number of free bits in the CHR bank mask.
 *
 * If a NES2.0 header is specified, and the requested amount of PRG-RAM
 * conflicts with the CHR bank mask, this function aborts.
 *
 * Assumes the calling object was provided a valid header during initialization.
 * Assumes CHR data has been initialized for the calling object.
 */
void Sxrom::LoadPrgRam(void) {
  // Determine if the rom supplied a valid PRG-RAM size.
  if (header_->header_type != NES2) {
    // Determine the max amount of ram that can be given to the rom from
    // the number of CHR banks used.
    if (chr_bank_mask_ <= 0x03U) {
      num_prg_ram_banks_ = MAX_RAM_BANKS;
    } else if (chr_bank_mask_ <= 0x07U) {
      num_prg_ram_banks_ = MAX_RAM_BANKS / 2U;
    } else {
      num_prg_ram_banks_ = 1U;
    }
  } else {
    // The mapper is NES 2.0, so we can use the specified PRG-RAM size.
    num_prg_ram_banks_ = header_->prg_ram_size / RAM_BANK_SIZE;
  }

  // Create the bank mask/shift from the number of banks.
  // The bank mask will use bits 3/2 of the chr registers, depending on space.
  prg_ram_bank_mask_ = CreateMask(num_prg_ram_banks_);
  prg_ram_bank_shift_ = (num_prg_ram_banks_ > 2U) ? 2U : 3U;
  prg_ram_bank_mask_ <<= prg_ram_bank_shift_;

  // Check for a conflict with CHR bank selection mask.
  if (prg_ram_bank_mask_ & chr_bank_mask_) {
    fprintf(stderr, "Error: the requested amount of PRG-RAM cannot be"
                    " addressed with the given CHR size.\n");
    abort();
  }

  // Allocate the PRG-RAM banks.
  for (size_t i = 0; i < num_prg_ram_banks_; i++) {
    prg_ram_[i] = RandNew(RAM_BANK_SIZE);
  }

  return;
}

/*
 * Reads the word at the specified address from memory, accounting
 * for MMIO and bank switching. If the address leads to an open bus,
 * the last value read is returned.
 *
 * Assumes that the connect function has been called on this object
 * with valid Cpu/Ppu/Apu objects.
 */
DataWord Sxrom::Read(DoubleWord addr) {
  // Determine which part of memory is being accessed, read the value,
  // and place it on the bus.
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
  } else if ((PRG_RAM_OFFSET <= addr) && (addr < PRG_ROM_A_OFFSET)
                                      && (num_prg_ram_banks_ > 0)
                                      && !(prg_reg_ & FLAG_PRG_RAM_DISABLE)) {
    // Read from PRG-RAM.
    bus_ = prg_ram_[prg_ram_bank_][addr & PRG_RAM_MASK];
  } else if ((PRG_ROM_A_OFFSET <= addr) && (addr < PRG_ROM_B_OFFSET)) {
    // Read from the low half of PRG-ROM.
    bus_ = prg_rom_[prg_rom_bank_a_][addr & PRG_ROM_MASK];
  } else if (addr >= PRG_ROM_B_OFFSET) {
    // Read from the high half of PRG-ROM.
    bus_ = prg_rom_[prg_rom_bank_b_][addr & PRG_ROM_MASK];
  }

  return bus_;
}

/*
 * Attempts to write the given value to requested address, updating
 * the controlling registers if PRG-ROM was written to.
 *
 * Assumes that Connect has been called on the calling object with valid
 * Cpu/Ppu/Apu objects.
 */
void Sxrom::Write(DoubleWord addr, DataWord val) {
  // Place the value on the bus.
  bus_ = val;

  // Attempt to write the value to memory.
  if (addr < PPU_OFFSET) {
    // Write to NES RAM.
    ram_[addr & RAM_MASK] = val;
  } else if ((PPU_OFFSET <= addr) && (addr < IO_OFFSET)) {
    // Write to PPU MMIO.
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
  } else if ((PRG_RAM_OFFSET <= addr) && (addr < PRG_ROM_A_OFFSET)
                                      && (num_prg_ram_banks_ > 0)
                                      && !(prg_reg_ & FLAG_PRG_RAM_DISABLE)) {
    // Write to cartridge RAM.
    prg_ram_[prg_ram_bank_][addr & PRG_RAM_MASK] = val;
  } else if (addr >= PRG_ROM_A_OFFSET) {
    // Write to the controlling registers for SxROM.
    UpdateRegisters(addr, val);
  }

  return;
}

/*
 * Updates the controlling registers for the calling object using the
 * given address and values.
 */
void Sxrom::UpdateRegisters(DoubleWord addr, DataWord val) {
  // Check if this update requested to reset the shift register and PRG-ROM
  // bank selection.
  if (val & FLAG_CONTROL_RESET) {
    shift_reg_ = SHIFT_BASE;
    control_reg_ = control_reg_ | CONTROL_RESET_MASK;
    return;
  }

  // If this update does not fill the shift register, we simply apply it
  // and return.
  if ((shift_reg_ & 1) != 1) {
    // Shift the LSB of the value into the MSB of the 5-bit shift register.
    shift_reg_ = ((shift_reg_ >> 1U) & 0xFU) | ((val & 1U) << 4U);
    return;
  }

  // Otherwise, we reset the shift register and apply the requested update.
  DataWord update = ((shift_reg_ >> 1U) & 0xFU) | ((val & 1U) << 4U);
  shift_reg_ = SHIFT_BASE;
  if ((CONTROL_UPDATE_OFFSET <= addr) && (addr < CHR_A_UPDATE_OFFSET)) {
    // Update the control register.
    UpdateControl(update);
  } else if ((CHR_A_UPDATE_OFFSET <= addr) && (addr < CHR_B_UPDATE_OFFSET)) {
    // Update the CHR0 register and copy the PRG selections to CHR1.
    chr_a_reg_ = update;
    chr_b_reg_ = (chr_b_reg_ & chr_bank_mask_) | (chr_a_reg_
               & (prg_ram_bank_mask_ | prg_rom_high_mask_));

    // Update the bank selections.
    UpdateChrBanks();
    prg_ram_bank_ = (chr_a_reg_ & prg_ram_bank_mask_) >> prg_ram_bank_shift_;
  } else if ((CHR_B_UPDATE_OFFSET <= addr) && (addr < PRG_UPDATE_OFFSET)) {
    // Update the CHR1 register and copy the PRG selections to CHR0.
    chr_b_reg_ = update;
    chr_a_reg_ = (chr_a_reg_ & chr_bank_mask_) | (chr_b_reg_
               & (prg_ram_bank_mask_ | prg_rom_high_mask_));

    // Update the bank selections.
    UpdateChrBanks();
    prg_ram_bank_ = (chr_b_reg_ & prg_ram_bank_mask_) >> prg_ram_bank_shift_;
  } else if (addr >= PRG_UPDATE_OFFSET) {
    // Update the PRG register.
    prg_reg_ = update;
    UpdatePrgRomBanks();
  }

  return;
}

/*
 * Updates the nametable, program rom, and chr addressing mode using the given
 * 5-bit control register update value.
 */
void Sxrom::UpdateControl(DataWord update) {
  // Update the control register.
  control_reg_ = update;

  // Update the nametable mirroring using the low two bits in the 5-bit update.
  switch(update & NAMETABLE_CONTROL_MASK) {
    case NAMETABLE_MIRROR_LOW:
      for (int i = 0; i < 4; i++) { nametable_[i] = nametable_bank_a_; }
      break;
    case NAMETABLE_MIRROR_HIGH:
      for (int i = 0; i < 4; i++) { nametable_[i] = nametable_bank_b_; }
      break;
    case NAMETABLE_MIRROR_VERT:
      nametable_[0] = nametable_bank_a_;
      nametable_[1] = nametable_bank_b_;
      nametable_[2] = nametable_bank_a_;
      nametable_[3] = nametable_bank_b_;
      break;
    case NAMETABLE_MIRROR_HORI:
      nametable_[0] = nametable_bank_a_;
      nametable_[1] = nametable_bank_a_;
      nametable_[2] = nametable_bank_b_;
      nametable_[3] = nametable_bank_b_;
      break;
  }

  // Update the CHR/PRG bank selections.
  UpdateChrBanks();
  UpdatePrgRomBanks();

  return;
}

/*
 * Updates the current bank selections using the regsiters in the
 * calling object.
 */
void Sxrom::UpdatePrgRomBanks(void) {
  // Caculate the current PRG-ROM bank selection.
  DataWord prg_bank = (chr_a_reg_ & prg_rom_high_mask_)
                    | (prg_reg_ & PRG_ROM_BANK_LOW_MASK);

  // Update the PRG-ROM bank selection using the method specified in the control
  // register and the current value of the PRG-ROM bank selection registers.
  switch(control_reg_ & PRG_ROM_CONTROL_MASK) {
    case PRG_ROM_MODE_32K:
    case PRG_ROM_MODE_32K_ALT:
      prg_rom_bank_a_ = prg_bank & (~1U);
      prg_rom_bank_b_ = prg_bank | 1U;
      break;
    case PRG_ROM_MODE_FIX_LOW:
      prg_rom_bank_a_ = 0;
      prg_rom_bank_b_ = prg_bank;
      break;
    case PRG_ROM_MODE_FIX_HIGH:
      prg_rom_bank_a_ = prg_bank;
      prg_rom_bank_b_ = num_prg_rom_banks_ - 1U;
      break;
  }

  return;
}

/*
 * Reloads the CHR bank selections from the CHR registers using the currently
 * selected bank switching mode in the control register.
 */
void Sxrom::UpdateChrBanks(void) {
  // Update the CHR bank selection using the high bit of the control register.
  if (control_reg_ & FLAG_CHR_MODE) {
    // 4KB bank mode.
    chr_bank_a_ = chr_a_reg_ & chr_bank_mask_;
    chr_bank_b_ = chr_b_reg_ & chr_bank_mask_;
  } else {
    // 8KB bank mode.
    chr_bank_a_ = chr_a_reg_ & chr_bank_mask_ & (~1U);
    chr_bank_b_ = chr_bank_a_ | 1U;
  }

  return;
}

/*
 * Reads a word from VRAM using the bank selection registers.
 */
DataWord Sxrom::VramRead(DoubleWord addr) {
  // Mask out any extra bits.
  addr &= VRAM_BUS_MASK;

  // Determine which part of VRAM is being accessed.
  if ((addr < NAMETABLE_OFFSET) && (addr & PATTERN_TABLE_HIGH_ACCESS_BIT)) {
    // CHR1 is being accessed.
    return pattern_table_[chr_bank_b_][addr & PATTERN_TABLE_MASK];
  } else if (addr < NAMETABLE_OFFSET) {
    // CHR0 is being accessed.
    return pattern_table_[chr_bank_a_][addr & PATTERN_TABLE_MASK];
  } else if (addr < PALETTE_OFFSET) {
    // Nametable is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> NAMETABLE_SELECT_SHIFT;
    return nametable_[table][addr & NAMETABLE_ADDR_MASK];
  } else {
    // Convert the address into an access to the palette data array.
    addr = (addr & PALETTE_BG_ACCESS_MASK) ? (addr & PALETTE_ADDR_MASK)
                                           : (addr & PALETTE_BG_MASK);
    return palette_data_[addr] >> PALETTE_NES_PIXEL_SHIFT;
  }
}

/*
 * Writes a word to VRAM at the requested memory location, if possible.
 * Uses the bank selection registers to address VRAM.
 */
void Sxrom::VramWrite(DoubleWord addr, DataWord val) {
  // Determine which part of VRAM is being accessed.
  if ((addr < NAMETABLE_OFFSET) && is_chr_ram_
     && (addr & PATTERN_TABLE_HIGH_ACCESS_BIT)) {
    // CHR1 is being accessed.
    pattern_table_[chr_bank_b_][addr & PATTERN_TABLE_MASK] = val;
  } else if ((addr < NAMETABLE_OFFSET) && is_chr_ram_) {
    // CHR0 is being accessed.
    pattern_table_[chr_bank_a_][addr & PATTERN_TABLE_MASK] = val;
  } else if ((NAMETABLE_OFFSET <= addr) && (addr < PALETTE_OFFSET)) {
    // Nametable is being accessed.
    DataWord table = (addr & NAMETABLE_SELECT_MASK) >> NAMETABLE_SELECT_SHIFT;
    nametable_[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if (addr >= PALETTE_OFFSET) {
    // Palette data is being accessed.
    PaletteWrite(addr, val);
  }

  return;
}

/*
 * Deletes the calling Sxrom object.
 */
Sxrom::~Sxrom(void) {
  // Free the CPU memory array.
  delete[] ram_;

  // Free the cartridge rom and ram.
  for (size_t i = 0; i < num_prg_rom_banks_; i++) { delete[] prg_rom_[i]; }
  for (size_t i = 0; i < num_prg_ram_banks_; i++) { delete[] prg_ram_[i]; }

  // Free the nametables.
  delete[] nametable_bank_a_;
  delete[] nametable_bank_b_;

  // Free the CHR data.
  for (size_t i = 0; i < num_chr_banks_; i++) { delete[] pattern_table_[i]; }

  return;
}
