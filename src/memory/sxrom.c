/*
 * Implementation of INES Mapper 1 (SxROM).
 *
 * TODO
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../util/util.h"
#include "./memory.h"
#include "./header.h"
#include "./sxrom.h"
#include "../util/data.h"

// Constants used to size and access memory.
#define MAX_ROM_BANKS 16U
#define ROM_BANK_SIZE 0x4000U
#define MAX_RAM_BANKS 4U
#define RAM_BANK_SIZE 0x2000U
#define PRG_RAM_OFFSET 0x6000U
#define PRG_ROM_A_OFFSET 0x8000U
#define PRG_ROM_B_OFFSET 0xC000U
#define PRG_RAM_MASK 0x1FFFU
#define PRG_ROM_MASK 0x3FFFU

// Constants used to control accesses to memory.
#define FLAG_CONTROL_RESET 0x80U
#define SHIFT_BASE 0x10U
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

// Constants used to size and access VRAM.
#define MAX_CHR_BANKS 8U
#define CHR_BANK_SIZE 0x1000U
#define MAX_SCREENS 4U
#define SCREEN_SIZE 0x400U

// Nes virtual memory data structure for sxrom (mapper 2).
typedef struct sxrom {
  // Cart memory.
  word_t *prg_rom[MAX_ROM_BANKS];
  word_t *prg_ram[MAX_RAM_BANKS];
  word_t num_prg_ram_banks;
  word_t num_prg_rom_banks;

  // PPU memory.
  word_t *pattern_table[MAX_CHR_BANKS];
  word_t num_chr_banks;
  bool is_chr_ram;
  word_t *nametable_bank_a;
  word_t *nametable_bank_b;
  word_t *nametable[MAX_SCREENS];

  // Controlling registers.
  word_t shift_reg;
  word_t control_reg;
  word_t chr_a_reg;
  word_t chr_b_reg;
  word_t prg_reg;

  // Bank selection registers.
  word_t chr_bank_a;
  word_t chr_bank_b;
  word_t prg_rom_bank_a;
  word_t prg_rom_bank_b;
  word_t prg_ram_bank;

  // Bank access masks.
  word_t chr_bank_mask;
  word_t prg_ram_bank_mask;
  word_t prg_ram_bank_shift;
  word_t prg_ram_disable_mask;
  word_t prg_rom_high_mask;
} sxrom_t;

/* Helper functions */
static void sxrom_load_prg_ram(memory_t *M);
static void sxrom_load_prg_rom(FILE *rom_file, memory_t *M);
static void sxrom_load_chr(FILE *rom_file, memory_t *M);
static void sxrom_update_registers(word_t val, dword_t addr, sxrom_t *M);
static void sxrom_update_control(word_t update, sxrom_t *M);
static void sxrom_update_prg_rom_banks(sxrom_t *M);
static void sxrom_update_chr_banks(sxrom_t *M);
static void sxrom_update_chr_a(word_t update, sxrom_t *M);
static void sxrom_update_chr_b(word_t update, sxrom_t *M);
static void sxrom_update_prg(word_t update, sxrom_t *M);

/*
 * Uses the header within the provided memory structure to create
 * an sxrom mapper structure and load the game data into the mapper.
 * The mapper structure and its functions are then stored within
 * the provided memory structure.
 *
 * Assumes the provided memory structure and rom file are non-null and valid.
 */
void sxrom_new(FILE *rom_file, memory_t *M) {
  // Allocate memory structure and set up its data.
  sxrom_t *map = xcalloc(1, sizeof(sxrom_t));
  M->map = (void*) map;
  M->read = &sxrom_read;
  M->write = &sxrom_write;
  M->vram_read = &sxrom_vram_read;
  M->vram_write = &sxrom_vram_write;
  M->free = &sxrom_free;

  // Load the rom data into memory.
  sxrom_load_prg_rom(rom_file, M);
  sxrom_load_chr(rom_file, M);

  // Set up the cart ram space.
  sxrom_load_prg_ram(M);

  // Setup the nametable in vram. The header mirroring bit is ignored in
  // this mapper, so we set the mirrored banks to a default value for now.
  map->nametable_bank_a = rand_alloc(sizeof(word_t) * SCREEN_SIZE);
  map->nametable_bank_b = rand_alloc(sizeof(word_t) * SCREEN_SIZE);
  map->nametable[0] = map->nametable_bank_a;
  map->nametable[1] = map->nametable_bank_a;
  map->nametable[2] = map->nametable_bank_a;
  map->nametable[3] = map->nametable_bank_a;

  return;
}

/*
 * Loads the given rom file into the sxrom memory structure.
 * Note that while this implementation assumes the banks will boot in mode 3,
 * with the upper half of rom fixed to the last bank and the lower half on bank
 * 0, this is not specified in original hardware.
 *
 * Assumes the provided rom_file is valid.
 * Assumes the provided memory structure and its mapper field are non-null.
 * Assumes the mapper field points to an sxrom mapper structure.
 */
static void sxrom_load_prg_rom(FILE *rom_file, memory_t *M) {
  // Cast back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;

  // Get the number of PRG-ROM banks, then load the rom into memory.
  map->num_prg_rom_banks = M->header->prg_rom_size / ROM_BANK_SIZE;
  fseek(rom_file, HEADER_SIZE, SEEK_SET);
  for (size_t i = 0; i < map->num_prg_rom_banks; i++) {
    map->prg_rom[i] = xmalloc(ROM_BANK_SIZE * sizeof(word_t));
    for (size_t j = 0; j < ROM_BANK_SIZE; j++) {
      map->prg_rom[i][j] = fgetc(rom_file);
    }
  }

  // Determine if the requested PRG-ROM size needs a fifth selection bit.
  if (map->num_prg_rom_banks > 16) {
    map->prg_rom_high_mask = 0x10;
  }

  // Setup the default bank mode.
  map->prg_rom_bank_a = 0;
  map->prg_rom_bank_b = map->num_prg_rom_banks - 1;

  return;
}

/*
 * Determine if the given rom is using CHR-ROM or CHR-RAM, then creates
 * the necessary banks. Loads in the CHR data if the board is using CHR-ROM.
 * Creates a mask for accessing the CHR control register.
 *
 * This function aborts if the created mask conflicts with the PRG-ROM mask.
 *
 * Assumes the provided rom file is valid.
 * Assumes the provided memory structure and its mapper field are non-null.
 * Assumes the mapper field points to a sxrom structure.
 * Assumes that the provided sxrom structure has had its PRG-ROM initialized.
 */
static void sxrom_load_chr(FILE *rom_file, memory_t *M) {
  // Cast the mapper back from the generic structure.
  sxrom_t *map = (sxrom_t*) M->map;

  // Check if the rom is using CHR-RAM, and allocate it if so.
  if (M->header->chr_ram_size > 0) {
    map->is_chr_ram = true;
    map->num_chr_banks = M->header->chr_ram_size / CHR_BANK_SIZE;
    for (size_t i = 0; i < map->num_chr_banks; i++) {
      map->pattern_table[i] = xmalloc(sizeof(word_t) * CHR_BANK_SIZE);
    }
  }

  // Otherwise, the rom is using CHR-ROM and we must load it into the mapper.
  map->is_chr_ram = false;
  map->num_chr_banks = M->header->chr_rom_size / CHR_BANK_SIZE;
  fseek(rom_file, HEADER_SIZE + M->header->prg_rom_size, SEEK_SET);
  for (size_t i = 0; i < map->num_chr_banks; i++) {
    map->pattern_table[i] = xmalloc(sizeof(word_t) * CHR_BANK_SIZE);
    for (size_t j = 0; j < CHR_BANK_SIZE; j++) {
      map->pattern_table[i][j] = fgetc(rom_file);
    }
  }

  // Calculate the mask to be used to select the CHR banks in the CHR control
  // register.
  map->chr_bank_mask = msb_word(map->num_chr_banks) - 1;

  // Check if the requested rom size conflicts with the requested number of
  // CHR banks.
  if (map->chr_bank_mask & map->prg_rom_high_mask) {
    fprintf(stderr, "Error: The requested amount of PRG-ROM cannot be "
                    "addressed with the given CHR size.\n");
    abort();
  }

  // Determine if the high CHR control bit is unused and should, thus,
  // be used to disable PRG-RAM.
  if (!map->prg_rom_high_mask && !(map->chr_bank_mask & 0x10)) {
    map->prg_ram_disable_mask = 0x10;
  }

  return;
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
    } else if (map->chr_bank_mask <= 0x0fU) {
      map->num_prg_ram_banks = 1U;
    }
  } else {
    // The mapper is NES 2.0, so we can use the specified PRG-RAM size.
    map->num_prg_ram_banks = M->header->prg_ram_size / RAM_BANK_SIZE;
  }

  // Create the bank mask/shift from the number of banks.
  // The bank mask will use bits 3/2 of the chr registers, depending on space.
  map->prg_ram_bank_mask = msb_word(map->num_prg_ram_banks) - 1U;
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
                               && (M->num_prg_ram_banks > 0)) {
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
                               && (M->num_prg_ram_banks > 0)) {
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
    // Update the CHR0 register.
    sxrom_update_chr_a(update, M);
  } else if ((CHR_B_UPDATE_OFFSET <= addr) && (addr < PRG_UPDATE_OFFSET)) {
    // Update the CHR1 register.
    sxrom_update_chr_b(update, M);
  } else if (addr >= PRG_UPDATE_OFFSET) {
    // Update the PRG register.
    sxrom_update_prg(update, M);
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
 * TODO
 */
static void sxrom_update_chr_banks(sxrom_t *M) {
  // Update the CHR bank selection using the high bit of the control register.
  if (M->control_reg & FLAG_CHR_MODE) {
    ;
  } else {
    ;
  }

  return;
}

/*
 * TODO
 */
static void sxrom_update_chr_a(word_t update, sxrom_t *M) {
  (void)update;
  (void)M;
  return;
}

/*
 * TODO
 */
static void sxrom_update_chr_b(word_t update, sxrom_t *M) {
  (void)update;
  (void)M;
  return;
}

/*
 * TODO
 */
static void sxrom_update_prg(word_t update, sxrom_t *M) {
  (void)update;
  (void)M;
  return;
}

/*
 * TODO
 */
word_t sxrom_vram_read(dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)addr;

  return 0;
}

/*
 * TODO
 */
void sxrom_vram_write(word_t val, dword_t addr, void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;
  (void)val;
  (void)addr;

  return;
}

/*
 * TODO
 */
void sxrom_free(void *map) {
  // Cast back from the generic structure.
  sxrom_t *M = (sxrom_t*) map;
  (void)M;

  return;
}
