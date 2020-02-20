#ifndef _NES_2A03
#define _NES_2A03

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/memory.h"
#include "../util/data.h"
#include "./cpu_operation.h"

// The CPU has a memory mapped register to start a DMA to OAM at this address.
#define CPU_DMA_ADDR 0x4014U

/*
 * Represents an emulated 6502 CPU. The state of the CPU is managed
 * using a CPU state queue. The CPU must be given a memory object to use
 * when it is initialized.
 */
class Cpu {
  private:
    // The size of each microcode sequence in the code table.
    static const size_t kInstSequenceSize_ = 8;

    /*
     * This structure represents the register file for the 6502 CPU, and
     * is used in the CPU emulation. The order of the fields in this file
     * must be consistent with the definitions in the operation header.
     *
     * Note that some of these fields are abstractions to make emulation more
     * efficient, and did not exist within the 6502.
     */
    typedef struct {
      DataWord pc_lo = 0;
      DataWord pc_hi = 0;
      DataWord addr_lo = 0;
      DataWord addr_hi = 0;
      DataWord temp1 = 0; // MDR and Pointer low.
      DataWord temp2 = 0; // Addr carry and Pointer high.
      DataWord s_lo = 0;
      DataWord s_hi = MEMORY_STACK_HIGH;
      DataWord vector_lo = MEMORY_VECTOR_LOW;
      DataWord vector_hi = MEMORY_VECTOR_HIGH;
      DataWord a = 0;
      DataWord x = 0;
      DataWord y = 0;
      DataWord p = 0x24U; // Bit 5 is always high and the I flag is set on init.
      DataWord inst = 0;
    } CpuRegFile;

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

    // Holds the registers inside the current CPU emulation.
    CpuRegFile *regs_;

    // Holds the table of microcode, which is indexed by instructions.
    CpuOperation *code_table_;

    // Used to buffer instructions which can have variable endings.
    CpuOperation inst_buffer_[kInstSequenceSize_];

    // Holds the microcode currently being executed, and the pointer to the
    // current instruction in it.
    CpuOperation *current_sequence_;
    CpuOperation current_operation_;
    size_t inst_pointer_ = 0;

    /* Helper functions for the CPU emulation */
    CpuOperation *LoadCodeTable(void);
    bool CheckNextCycle(void);
    void ExecuteDma(void);
    bool CanPoll(void);
    void RunOperation(void);
    void RunMemoryOperation(CpuOperation &op);
    void RunDataOperation(CpuOperation &op);
    void Fetch(CpuOperation &op);
    void PollNmiLine(void);
    void PollIrqLine(void);

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
