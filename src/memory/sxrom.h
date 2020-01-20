#ifndef _NES_SXROM
#define _NES_SXROM

#include <cstdlib>
#include <cstdint>

#include "../util/data.h"
#include "./memory.h"
#include "./header.h"

// The defined mapper number that this memory system belongs to.
#define SXROM_MAPPER 1

// The maximum number of banks for each memory field.
#define SXROM_MAX_ROM_BANKS 32U
#define SXROM_MAX_RAM_BANKS 4U
#define SXROM_MAX_CHR_BANKS 32U
#define SXROM_MAX_SCREENS 4U

// The default value in the shift register, used to track when it fills.
#define SXROM_SHIFT_BASE 0x10U

class Sxrom : public Memory {
  private:
    // Used to emulate open bus behavior. Stores the last value
    // read from/written to memory.
    DataWord bus_ = 0;

    // NES system ram.
    DataWord *ram_;

    // Cartridge memory space for this mapper.
    DataWord *prg_rom_[SXROM_MAX_ROM_BANKS];
    DataWord *prg_ram_[SXROM_MAX_RAM_BANKS];
    DataWord num_prg_ram_banks_;
    DataWord num_prg_rom_banks_;

    // PPU memory space.
    DataWord *pattern_table_[SXROM_MAX_CHR_BANKS];
    DataWord num_chr_banks_;
    bool is_chr_ram_;
    DataWord *nametable_bank_a_;
    DataWord *nametable_bank_b_;
    DataWord *nametable_[SXROM_MAX_SCREENS];

    // Controlling registers. The emulated program can use these to change
    // the settings of the SxROM mapper.
    DataWord shift_reg_ = SXROM_SHIFT_BASE;
    DataWord control_reg_ = 0;
    DataWord chr_a_reg_ = 0;
    DataWord chr_b_reg_ = 0;
    DataWord prg_reg_ = 0;

    // Bank selection registers.
    DataWord chr_bank_a_ = 0;
    DataWord chr_bank_b_ = 0;
    DataWord prg_rom_bank_a_ = 0;
    DataWord prg_rom_bank_b_;
    DataWord prg_ram_bank_ = 0;

    // Bank access masks.
    DataWord chr_bank_mask_;
    DataWord prg_ram_bank_mask_;
    DataWord prg_ram_bank_shift_;
    DataWord prg_rom_high_mask_;

    // Helper functions for this mapper.
    void LoadPrgRam(void);
    void LoadPrgRom(FILE *rom_file);
    void LoadChr(FILE *rom_file);
    DataWord CreateMask(DataWord items);
    void UpdateRegisters(DoubleWord addr, DataWord val);
    void UpdateControl(DataWord update);
    void UpdatePrgRomBanks(void);
    void UpdateChrBanks(void);

  public:
    // Functions implemented for the abstract class Memory.
    DataWord Read(DoubleWord addr);
    void Write(DoubleWord addr, DataWord val);
    bool CheckRead(DoubleWord addr);
    bool CheckWrite(DoubleWord addr);
    DataWord VramRead(DoubleWord addr);
    void VramWrite(DoubleWord addr, DataWord val);

    Sxrom(FILE *rom_file, RomHeader *header);
    ~Sxrom(void);
};

#endif
