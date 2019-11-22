/*
 * Implementation of INES Mapper 1 (SxROM).
 *
 * TODO
 */

#include "./sxrom.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/util.h"
#include "../util/data.h"
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
  if (num_prg_rom_banks_ > 16) { prg_rom_high_mask_ = 0x10; }

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
 * Assumes the memory structure and its mapper field are non-null.
 * Assumes the header provided by the memory structure is valid.
 * Assumes CHR data has been initialized for the given SxROM structure.
 */
static void sxrom_load_prg_ram(memory_t *M) {
  // Cast back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;

  // Determine if the rom supplied a valid PRG-RAM size.
  if (M->header->header_type != NES2) {
    // Determine the max amount of ram that can be given to the rom from
    // the number of CHR banks used.
    if (map->chr_bank_mask <= 0x03U) {
      map->num_prg_ram_banks = MAX_RAM_BANKS;
    } else if (map->chr_bank_mask <= 0x07U) {
      map->num_prg_ram_banks = MAX_RAM_BANKS / 2U;
    } else {
      map->num_prg_ram_banks = 1U;
    }
  } else {
    // The mapper is NES 2.0, so we can use the specified PRG-RAM size.
    map->num_prg_ram_banks = M->header->prg_ram_size / RAM_BANK_SIZE;
  }

  // Create the bank mask/shift from the number of banks.
  // The bank mask will use bits 3/2 of the chr registers, depending on space.
  map->prg_ram_bank_mask = sxrom_create_mask(map->num_prg_ram_banks);
  map->prg_ram_bank_shift = (map->num_prg_ram_banks > 2U) ? 2U : 3U;
  map->prg_ram_bank_mask <<= map->prg_ram_bank_shift;

  // Check for a conflict with CHR bank selection mask.
  if (map->prg_ram_bank_mask & map->chr_bank_mask) {
    fprintf(stderr, "Error: the requested amount of PRG-RAM cannot be"
                    " addressed with the given CHR size.\n");
    abort();
  }

  // Allocate the PRG-RAM banks.
  for (size_t i = 0; i < map->num_prg_ram_banks; i++) {
    map->prg_ram[i] = rand_alloc(RAM_BANK_SIZE * sizeof(word_t));
  }

  return;
}

/*
 * Reads the word at the specified address from memory using the profided mapper
 * structure.
 *
 * Assumes the provided pointer is non-null and points to a valid sxrom
 * structure.
 */
word_t sxrom_read(dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  sxrom_t *M = (sxrom_t*) map;

  // Determine which part of memory is being accessed, read the value,
  // and place it on the bus.
  if ((PRG_RAM_OFFSET <= addr) && (addr < PRG_ROM_A_OFFSET)
                               && (M->num_prg_ram_banks > 0)
                               && !(M->prg_reg & FLAG_PRG_RAM_DISABLE)) {
    // Read from PRG-RAM.
    return M->prg_ram[M->prg_ram_bank][addr & PRG_RAM_MASK];
  } else if ((PRG_ROM_A_OFFSET <= addr) && (addr < PRG_ROM_B_OFFSET)) {
    // Read from the low half of PRG-ROM.
    return M->prg_rom[M->prg_rom_bank_a][addr & PRG_ROM_MASK];
  } else if (addr >= PRG_ROM_B_OFFSET) {
    // Read from the high half of PRG-ROM.
    return M->prg_rom[M->prg_rom_bank_b][addr & PRG_ROM_MASK];
  }

  return memory_bus;
}

/*
 * Attempts to write the given value to requested address, updating
 * the controlling registers if PRG-ROM was written to.
 *
 * Assumes the provided pointer is non-null and points to a valid sxrom
 * structure.
 */
void sxrom_write(word_t val, dword_t addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  sxrom_t *M = (sxrom_t*) map;

  // Load the bus with the requested value, and attempt to write that value to
  // memory.
  if ((PRG_RAM_OFFSET <= addr) && (addr < PRG_ROM_A_OFFSET)
                               && (M->num_prg_ram_banks > 0)
                               && !(M->prg_reg & FLAG_PRG_RAM_DISABLE)) {
    M->prg_ram[M->prg_ram_bank][addr & PRG_RAM_MASK] = val;
  } else if (addr >= PRG_ROM_A_OFFSET) {
    sxrom_update_registers(val, addr, M);
  }

  return;
}

/*
 * Updates the controlling registers for the provided sxrom structure
 * using the given address and values.
 *
 * Assumes the provided sxrom structure is non-null and valid.
 */
static void sxrom_update_registers(word_t val, dword_t addr, sxrom_t *M) {
  // Check if this update requested to reset the shift register and PRG-ROM
  // bank selection.
  if (val & FLAG_CONTROL_RESET) {
    M->shift_reg = SHIFT_BASE;
    M->control_reg = M->control_reg | CONTROL_RESET_MASK;
    return;
  }

  // If this update does not fill the shift register, we simply apply it
  // and return.
  if ((M->shift_reg & 1) != 1) {
    // Shift the LSB of the value into the MSB of the 5-bit shift register.
    M->shift_reg = ((M->shift_reg >> 1U) & 0xFU) | ((val & 1U) << 4U);
    return;
  }

  // Otherwise, we reset the shift register and apply the requested update.
  word_t update = ((M->shift_reg >> 1U) & 0xFU) | ((val & 1U) << 4U);
  M->shift_reg = SHIFT_BASE;
  if ((CONTROL_UPDATE_OFFSET <= addr) && (addr < CHR_A_UPDATE_OFFSET)) {
    // Update the control register.
    sxrom_update_control(update, M);
  } else if ((CHR_A_UPDATE_OFFSET <= addr) && (addr < CHR_B_UPDATE_OFFSET)) {
    // Update the CHR0 register and copy the PRG selections to CHR1.
    M->chr_a_reg = update;
    M->chr_b_reg = (M->chr_b_reg & M->chr_bank_mask) | (M->chr_a_reg
                 & (M->prg_ram_bank_mask | M->prg_rom_high_mask));

    // Update the bank selections.
    sxrom_update_chr_banks(M);
    M->prg_ram_bank = (M->chr_a_reg & M->prg_ram_bank_mask)
                    >> M->prg_ram_bank_shift;
  } else if ((CHR_B_UPDATE_OFFSET <= addr) && (addr < PRG_UPDATE_OFFSET)) {
    // Update the CHR1 register and copy the PRG selections to CHR0.
    M->chr_b_reg = update;
    M->chr_a_reg = (M->chr_a_reg & M->chr_bank_mask) | (M->chr_b_reg
                 & (M->prg_ram_bank_mask | M->prg_rom_high_mask));

    // Update the bank selections.
    sxrom_update_chr_banks(M);
    M->prg_ram_bank = (M->chr_b_reg & M->prg_ram_bank_mask)
                    >> M->prg_ram_bank_shift;
  } else if (addr >= PRG_UPDATE_OFFSET) {
    // Update the PRG register.
    M->prg_reg = update;
    sxrom_update_prg_rom_banks(M);
  }

  return;
}

/*
 * Updates the nametable, program rom, and chr addressing mode using the given
 * 5-bit control register update value.
 *
 * Assumes the provided sxrom structure is non-null and valid.
 */
static void sxrom_update_control(word_t update, sxrom_t *M) {
  // Update the control register.
  M->control_reg = update;

  // Update the nametable mirroring using the low two bits in the 5-bit update.
  switch(update & NAMETABLE_CONTROL_MASK) {
    case NAMETABLE_MIRROR_LOW:
      for (int i = 0; i < 4; i++) { M->nametable[i] = M->nametable_bank_a; }
      break;
    case NAMETABLE_MIRROR_HIGH:
      for (int i = 0; i < 4; i++) { M->nametable[i] = M->nametable_bank_b; }
      break;
    case NAMETABLE_MIRROR_VERT:
      M->nametable[0] = M->nametable_bank_a;
      M->nametable[1] = M->nametable_bank_b;
      M->nametable[2] = M->nametable_bank_a;
      M->nametable[3] = M->nametable_bank_b;
      break;
    case NAMETABLE_MIRROR_HORI:
      M->nametable[0] = M->nametable_bank_a;
      M->nametable[1] = M->nametable_bank_a;
      M->nametable[2] = M->nametable_bank_b;
      M->nametable[3] = M->nametable_bank_b;
      break;
  }

  // Update the CHR/PRG bank selections.
  sxrom_update_chr_banks(M);
  sxrom_update_prg_rom_banks(M);

  return;
}

/*
 * Updates the current bank selections using the regsiters in the provided
 * SxROM memory structure.
 *
 * Assumes the provided memory structure is non-null and valid.
 */
static void sxrom_update_prg_rom_banks(sxrom_t *M) {
  // Caculate the current PRG-ROM bank selection.
  word_t prg_bank = (M->chr_a_reg & M->prg_rom_high_mask)
                  | (M->prg_reg & PRG_ROM_BANK_LOW_MASK);

  // Update the PRG-ROM bank selection using the method specified in the control
  // register and the current value of the PRG-ROM bank selection registers.
  switch(M->control_reg & PRG_ROM_CONTROL_MASK) {
    case PRG_ROM_MODE_32K:
    case PRG_ROM_MODE_32K_ALT:
      M->prg_rom_bank_a = prg_bank & (~1U);
      M->prg_rom_bank_b = prg_bank | 1U;
      break;
    case PRG_ROM_MODE_FIX_LOW:
      M->prg_rom_bank_a = 0;
      M->prg_rom_bank_b = prg_bank;
      break;
    case PRG_ROM_MODE_FIX_HIGH:
      M->prg_rom_bank_a = prg_bank;
      M->prg_rom_bank_b = M->num_prg_rom_banks - 1U;
      break;
  }

  return;
}

/*
 * Reloads the CHR bank selections from the CHR registers using the currently
 * selected bank switching mode in the control register.
 *
 * Assumes the provided SxROM structure is non-null and valid.
 */
static void sxrom_update_chr_banks(sxrom_t *M) {
  // Update the CHR bank selection using the high bit of the control register.
  if (M->control_reg & FLAG_CHR_MODE) {
    // 4KB bank mode.
    M->chr_bank_a = M->chr_a_reg & M->chr_bank_mask;
    M->chr_bank_b = M->chr_b_reg & M->chr_bank_mask;
  } else {
    // 8KB bank mode.
    M->chr_bank_a = M->chr_a_reg & M->chr_bank_mask & (~1U);
    M->chr_bank_b = M->chr_bank_a | 1U;
  }

  return;
}

/*
 * Reads a word from VRAM using the bank selection registers.
 *
 * Assumes the provided pointer is non-null.
 * Assumes the provided pointer is of type sxrom_t and is valid.
 */
word_t sxrom_vram_read(dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Nametable is being accessed.
    word_t table = (addr & NAMETABLE_SELECT_MASK) >> NAMETABLE_SELECT_SHIFT;
    return M->nametable[table][addr & NAMETABLE_ADDR_MASK];
  } else if (addr & PATTERN_TABLE_HIGH_ACCESS_BIT) {
    // CHR1 is being accessed.
    return M->pattern_table[M->chr_bank_b][addr & PATTERN_TABLE_MASK];
  } else {
    // CHR0 is being accessed.
    return M->pattern_table[M->chr_bank_a][addr & PATTERN_TABLE_MASK];
  }
}

/*
 * Writes a word to VRAM at the requested memory location, if possible.
 * Uses the bank selection registers to address VRAM.
 *
 * Assumes the provided pointer is non-null.
 * Assumes the provided pointer is of type sxrom_t and is valid.
 */
void sxrom_vram_write(word_t val, dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;

  // Determine which part of VRAM is being accessed.
  if (addr & NAMETABLE_ACCESS_BIT) {
    // Nametable is being accessed.
    word_t table = (addr & NAMETABLE_SELECT_MASK) >> NAMETABLE_SELECT_SHIFT;
    M->nametable[table][addr & NAMETABLE_ADDR_MASK] = val;
  } else if ((addr & PATTERN_TABLE_HIGH_ACCESS_BIT) && M->is_chr_ram) {
    // CHR1 is being accessed.
    M->pattern_table[M->chr_bank_b][addr & PATTERN_TABLE_MASK] = val;
  } else if (M->is_chr_ram) {
    // CHR0 is being accessed.
    M->pattern_table[M->chr_bank_a][addr & PATTERN_TABLE_MASK] = val;
  }

  return;
}

/*
 * Frees the provided SxROM structure.
 *
 * Assumes the provided pointer is non-null.
 * Assumes the provided pointer is of type sxrom_t and is valid.
 */
void sxrom_free(void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;

  // Free the cartridge rom and ram.
  for (size_t i = 0; i < M->num_prg_rom_banks; i++) { free(M->prg_rom[i]); }
  for (size_t i = 0; i < M->num_prg_ram_banks; i++) { free(M->prg_ram[i]); }

  // Free the nametables.
  free(M->nametable_bank_a);
  free(M->nametable_bank_b);

  // Free the CHR data.
  for (size_t i = 0; i < M->num_chr_banks; i++) { free(M->pattern_table[i]); }

  // Free the structure itself.
  free(M);

  return;
}
