#ifndef _NES_2A03
#define _NES_2A03

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/memory.h"
#include "../util/data.h"
#include "./cpu_state.h"
#include "./cpu_status.h"

// The CPU has a memory mapped register to start a DMA to OAM at this address.
#define CPU_DMA_ADDR 0x4014U

/*
 * This structure represents the register file for the 6502 CPU, and
 * is used in the CPU emulation.
 */
typedef struct {
  DataWord a;
  DataWord x;
  DataWord y;
  DataWord inst;
  DataWord mdr;
  DataWord addr_carry;
  MultiWord s;
  MultiWord pc;
  MultiWord addr;
  MultiWord ptr;
  CpuStatus p;
} CpuRegFile;

/*
 * TODO
 */
class Cpu {
  private:
    // Interrupt bools, which can be set inside of CPU operations.
    bool nmi_edge_;
    bool irq_level_;
    // Some instructions need to poll on one cycle and then interrupt later.
    // This variable is used to facilitate that.
    bool irq_ready_;

    // Used for DMA transfers to PPU OAM.
    bool cycle_even_;
    size_t dma_cycles_remaining_;
    MultiWord dma_addr_;

    // Holds the associated memory object, which is used to access
    // memory during the emulation.
    Memory *memory_;

    // Used to manage the current state of the CPU emulation.
    CpuState *state_;

    // Holds the registers inside the current CPU emulation.
    CpuRegFile *regs_

    /* Helper functions for the CPU emulation */
    bool CanPoll(void);
    void ExecuteDma(void);
    void Fetch(OperationCycle *op_cycle);
    void DecodeInst(void);
    void DecodeIzpx(CpuOperation *op);
    void DecodeZp(CpuOperation *op);
    void DecodeImm(CpuOperation *op);
    void DecodeAbs(CpuOperation *op);
    void DecodeIzpY(CpuOperation *op);
    void DecodeZpx(CpuOperation *op);
    void DecodeZpy(CpuOperation *op);
    void DecodeAbx(CpuOperation *op);
    void DecodeAby(CpuOperation *op);
    void DecodeNomem(CpuOperation *op);
    void DecodeRwZp(CpuOperation *op);
    void DecodeRwAbs(CpuOperation *op);
    void DecodeRwZpx(CpuOperation *op);
    void DecodeRwAbx(CpuOperation *op);
    void DecodeWIzpx(CpuOperation *op);
    void DecodeWZp(CpuOperation *op);
    void DecodeWAbs(CpuOperation *op);
    void DecodeWIzpY(CpuOperation *op);
    void DecodeWZpx(CpuOperation *op);
    void DecodeWZpy(CpuOperation *op);
    void DecodeWAbx(CpuOperation *op);
    void DecodeWAby(CpuOperation *op);
    void DecodePush(CpuOperation *op);
    void DecodePull(CpuOperation *op);
    void PollNmiLine(void);
    void PollIrqLine(void);

  public:
    // Interrupt lines, which can be set by the PPU/APU.
    DataWord irq_line_;
    bool nmi_line_;

    // Creates a new CPU object.
    Cpu(Memory *memory);

    // Executes the next cycle of the cpu emulation.
    void RunCycle(void);

    // Starts a DMA transfer from CPU memory to PPU OAM.
    void StartDma(DataWord addr);

    // Deletes the CPU object. The associated memory object is not deleted.
    ~Cpu(void);
};

#endif
