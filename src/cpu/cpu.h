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
 *
 * Note that some of these fields are abstractions to make emulation more
 * efficient, and did not exist within the 6502.
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
 * Represents an emulated 6502 CPU. The state of the CPU is managed
 * using a CPU state queue. The CPU must be given a memory object to use
 * when it is initialized.
 */
class Cpu {
  private:
    // Used to edge detect the NMI signal.
    bool nmi_prev_ = false;

    // Interrupt bools, which can be set inside of CPU operations.
    bool nmi_edge_ = false;
    bool irq_level_ = false;
    // Some instructions need to poll on one cycle and then interrupt later.
    // This variable is used to facilitate that.
    bool irq_ready_ = false;

    // Used for DMA transfers to PPU OAM.
    bool cycle_even_ = false;
    DataWord dma_mdr_ = 0;
    size_t dma_cycles_remaining_ = 0;
    MultiWord dma_addr_ = { 0 };

    // Holds the associated memory object, which is used to access
    // memory during the emulation.
    Memory *memory_;

    // Used to manage the current state of the CPU emulation.
    CpuState *state_;

    // Holds the registers inside the current CPU emulation.
    CpuRegFile *regs_;

    /* Helper functions for the CPU emulation */
    bool CanPoll(void);
    void ExecuteDma(void);
    void Fetch(OperationCycle *op_cycle);
    void DecodeInst(void);
    void DecodeIzpx(CpuOperation op);
    void DecodeZp(CpuOperation op);
    void DecodeImm(CpuOperation op);
    void DecodeAbs(CpuOperation op);
    void DecodeIzpY(CpuOperation op);
    void DecodeZpx(CpuOperation op);
    void DecodeZpy(CpuOperation op);
    void DecodeAbx(CpuOperation op);
    void DecodeAby(CpuOperation op);
    void DecodeNomem(CpuOperation op);
    void DecodeRwZp(CpuOperation op);
    void DecodeRwAbs(CpuOperation op);
    void DecodeRwZpx(CpuOperation op);
    void DecodeRwAbx(CpuOperation op);
    void DecodeWIzpx(CpuOperation op);
    void DecodeWZp(CpuOperation op);
    void DecodeWAbs(CpuOperation op);
    void DecodeWIzpY(CpuOperation op);
    void DecodeWZpx(CpuOperation op);
    void DecodeWZpy(CpuOperation op);
    void DecodeWAbx(CpuOperation op);
    void DecodeWAby(CpuOperation op);
    void DecodePush(CpuOperation op);
    void DecodePull(CpuOperation op);
    void PollNmiLine(void);
    void PollIrqLine(void);

    /* Data operations */
    void Nop(void);
    void DataIncS(void);
    void DataIncX(void);
    void DataIncY(void);
    void DataIncMdr(void);
    void DataDecS(void);
    void DataDecX(void);
    void DataDecY(void);
    void DataDecMdr(void);
    void DataMovAX(void);
    void DataMovAY(void);
    void DataMovSX(void);
    void DataMovXA(void);
    void DataMovXS(void);
    void DataMovYA(void);
    void DataMovMdrPcl(void);
    void DataMovMdrA(void);
    void DataMovMdrX(void);
    void DataMovMdrY(void);
    void DataClc(void);
    void DataCld(void);
    void DataCli(void);
    void DataClv(void);
    void DataSec(void);
    void DataSed(void);
    void DataSei(void);
    void DataCmpMdrA(void);
    void DataCmpMdrX(void);
    void DataCmpMdrY(void);
    void DataAslMdr(void);
    void DataAslA(void);
    void DataLsrMdr(void);
    void DataLsrA(void);
    void DataRolMdr(void);
    void DataRolA(void);
    void DataRorMdr(void);
    void DataRorA(void);
    void DataEorMdrA(void);
    void DataAndMdrA(void);
    void DataOraMdrA(void);
    void DataAdcMdrA(void);
    void DataSbcMdrA(void);
    void DataBitMdrA(void);
    void DataAddAddrlX(void);
    void DataAddAddrlY(void);
    void DataAddPtrlX(void);
    void DataFixaAddrh(void);
    void DataFixAddrh(void);
    void DataFixPch(void);
    void DataBranch(void);

    /* Memory operations */
    void MemFetch(void);
    void MemReadPcNodest(void);
    void MemReadPcMdr(void);
    void MemReadPcPch(void);
    void MemReadPcZpAddr(void);
    void MemReadPcAddrl(void);
    void MemReadPcAddrh(void);
    void MemReadPcZpPtr(void);
    void MemReadPcPtrl(void);
    void MemReadPcPtrh(void);
    void MemReadAddrMdr(void);
    void MemReadPtrMdr(void);
    void MemReadPtrAddrl(void);
    void MemReadPtr1Addrh(void);
    void MemReadPtr1Pch(void);
    void MemWriteMdrAddr(void);
    void MemWriteAAddr(void);
    void MemWriteXAddr(void);
    void MemWriteYAddr(void);
    void MemPushPcl(void);
    void MemPushPch(void);
    void MemPushA(void);
    void MemPushP(void);
    void MemPushPB(void);
    void MemBrk(void);
    void MemIrq(void);
    void MemPullPcl(void);
    void MemPullPch(void);
    void MemPullA(void);
    void MemPullP(void);
    void MemNmiPcl(void);
    void MemNmiPch(void);
    void MemResetPcl(void);
    void MemResetPch(void);
    void MemIrqPcl(void);
    void MemIrqPch(void);

    /* Operations used to check if a memory access is safe */
    bool CheckPcRead(void);
    bool CheckAddrRead(void);
    bool CheckAddrWrite(void);
    bool CheckPtrRead(void);
    bool CheckPtr1Read(void);

  public:
    // Interrupt lines, which can be set by the PPU/APU.
    DataWord irq_line_ = 0;
    bool nmi_line_ = false;

    // Creates a new CPU object.
    Cpu(void);

    // Connects a Memory object to the calling CPU object.
    void Connect(Memory *memory);

    // Reads the reset vector from memory so that the CPU can start emulating
    // cycles. Connect() must be called before this function.
    void Power(void);

    // Executes the CPU emulation until either the requested number of cycles
    // have been executed, or the CPU must sync with the APU/PPU.
    size_t RunSchedule(size_t cycles, size_t *syncs);

    // Executes the next cycle of the cpu emulation.
    // Connect() and Power() must be called before this function.
    void RunCycle(void);

    // Starts a DMA transfer from CPU memory to PPU OAM.
    void StartDma(DataWord addr);

    // Deletes the CPU object. The associated memory object is not deleted.
    ~Cpu(void);
};

#endif
