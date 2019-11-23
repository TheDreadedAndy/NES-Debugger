#ifndef _NES_NROM
#define _NES_NROM

#include <cstdlib>
#include <cstdint>

#include "../util/data.h"
#include "./memory.h"
#include "./header.h"

// The defined mapper number that this memory system belongs to.
#define NROM_MAPPER 0

// The maximum number of cartridge banks for this mapper.
#define NROM_MAX_BANKS 2U

// The maximum number of nametable screens for this mapper.
#define NROM_MAX_SCREENS 4U

class Nrom : public Memory {
  private:
    // Used to emulate open bus behavior. Stores the last value read from/
    // written to memory.
    DataWord bus_ = 0;

    // NES system RAM.
    DataWord *ram_;

    // Cart memory.
    DataWord *bat_;
    DataWord *cart_[NROM_MAX_BANKS];

    // PPU memory.
    DataWord *pattern_table_;
    bool is_chr_ram_;
    DataWord *nametable_[NROM_MAX_SCREENS];

    // Helper functions for this implementation of memory.
    void LoadPrg(FILE *rom_file);
    void LoadChr(FILE *rom_file);

  public:
    // Functions implemented for the abstact class Memory.
    DataWord Read(DoubleWord addr);
    void Write(DoubleWord addr, DataWord val);
    DataWord VramRead(DoubleWord addr);
    void VramWrite(DoubleWord addr, DataWord val);

    Nrom(FILE *rom_file, RomHeader *header);
    ~Nrom(void);
};

#endif
