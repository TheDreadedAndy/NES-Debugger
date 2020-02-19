#ifndef _NES_UXROM
#define _NES_UXROM

#include <cstdlib>
#include <cstdint>

#include "../util/data.h"
#include "../config/config.h"
#include "./memory.h"
#include "./header.h"

// The defined mapper number that this memory system belongs to.
#define UXROM_MAPPER 2

// The maximum number of cart banks supported by this mapper.
#define UXROM_MAX_BANKS 16U

// The maximum number of nametable screens exposed by memory.
#define UXROM_MAX_SCREENS 4U

class Uxrom : public Memory {
  private:
    // Used to emulate open bus behavior. Stores the last value read
    // from system memory.
    DataWord bus_ = 0;

    // NES system ram.
    DataWord *ram_;

    // Cart memory.
    DataWord *bat_;
    DataWord *cart_[UXROM_MAX_BANKS];
    DataWord current_bank_ = 0;
    // Should always be fixed to the final bank used.
    DataWord fixed_bank_;
    DataWord bank_mask_;

    // PPU memory.
    DataWord *pattern_table_;
    bool is_chr_ram_;
    DataWord *nametable_[UXROM_MAX_SCREENS];

    // Helper functions for this class.
    void LoadPrg(FILE *rom_file);
    void LoadChr(FILE *rom_file);

  public:
    // Functions implemented for the abstract class Memory.
    DataWord Read(DoubleWord addr);
    void Write(DoubleWord addr, DataWord val);
    bool CheckRead(DoubleWord addr);
    bool CheckWrite(DoubleWord addr);
    DataWord VramRead(DoubleWord addr);
    void VramWrite(DoubleWord addr, DataWord val);

    Uxrom(FILE *rom_file, RomHeader *header, Config *config);
    ~Uxrom(void);
};

#endif
