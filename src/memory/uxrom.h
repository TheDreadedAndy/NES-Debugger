#include <stdlib.h>
#include <stdint.h>
#include "./memory.h"
#include "./header.h"
#include "../util/data.h"

#ifndef _NES_UXROM
#define _NES_UXROM

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
    DataWord bus;

    // NES system ram.
    DataWord *ram;

    // Cart memory.
    DataWord *bat;
    DataWord *cart[UXROM_MAX_BANKS];
    DataWord current_bank;
    // Should always be fixed to the final bank used.
    DataWord fixed_bank;
    DataWord bank_mask;

    // PPU memory.
    DataWord *pattern_table;
    bool is_chr_ram;
    DataWord *nametable[UXROM_MAX_SCREENS];

    // Used to expose colors to the rendering system as an optimization.
    uint32_t *palette_data;

    // Helper functions for this class.
    void LoadPrg(FILE *rom_file);
    void LoadChr(FILE *rom_file);

  public:
    Uxrom(FILE *rom_file, header_t *rom_header);
    ~Uxrom();
};

#endif
