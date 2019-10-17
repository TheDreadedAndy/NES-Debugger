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
 * TODO
 *
 * Takes in an address and a generic mapper pointer.
 * Uses these bytes to address the memory in the mapper.
 *
 * Assumes that the pointer is non-null and points to a valid
 * UxROM mapper.
 */
DataWord Uxrom::Read(DoubleWord addr) {
  // Detect where in memory we need to access and do so.
  if (addr < BAT_OFFSET) {
    return memory_bus;
  } else if (addr < BANK_OFFSET) {
    return M->bat[addr & BAT_MASK];
  } else if (addr < FIXED_BANK_OFFSET) {
    return M->cart[M->current_bank][addr & BANK_ADDR_MASK];
  } else {
    return M->cart[M->fixed_bank][addr & BANK_ADDR_MASK];
  }
}

/*
 * Takes in an address, a value, and a memory mapper.
 * Uses the address to write the value to the mapper.
 *
 * Assumes that the mapper pointer is non-null and points
 * to a valid UxROM mapper.
 */
void uxrom_write(DataWord val, DoubleWord addr, void *map) {
  // Cast back from generic pointer to the memory structure.
  uxrom_t *M = (uxrom_t*) map;

  // Detect where in memory we need to access and do so.
  if ((BAT_OFFSET <= addr) && (addr < BANK_OFFSET)) {
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
