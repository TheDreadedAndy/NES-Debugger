#ifndef _NES_2A03
#define _NES_2A03

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/header.h"
#include "../util/data.h"
#include "./state.h"

// The CPU has a memory mapped register to start a DMA to OAM at this address.
#define CPU_DMA_ADDR 0x4014U

/*
 * TODO
 */
class Cpu {
  private:
    // Interrupt bools, which can be set inside of CPU operations.
    bool nmi_edge_;
    bool irq_ready_;

  public:
    // Interrupt lines, which can be set by the PPU/APU.
    DataWord irq_line_;
    bool nmi_line_;
};

// Interrupt lines, which can be set by the PPU/APU
extern DataWord irq_line;
extern bool nmi_line;

// Interrupt bools, which can be used by the micro ops.
extern bool nmi_edge, irq_ready;

// Initializes the cpu, so that cycles may be executed.
void cpu_init(FILE *rom_file, RomHeader *header);

// Executes the next cycle of the 2A03.
void cpu_run_cycle(void);

// Starts a DMA transfer from CPU memory to OAM.
void cpu_start_dma(DataWord addr);

// Fetches the next instruction in the emulation. Should only be called from
// micro ops.
void cpu_fetch(micro_t *micro);

// Frees anything related to the cpu emulation.
void cpu_free(void);

#endif