/*
 * This file contains the interpreter for the emulation of the NES's 6502
 * CPU. The interpreter is emulated one cycle at a time by calling the
 * function RunCycle() on a cpu object.
 *
 * Instructions are run using a state queue, found in cpu_state.cc, which is
 * filled by the fetch phase of an instruction. The queue contains micro
 * instructions for data and memory operations, which are executed in pairs
 * of memory and data each cycle.
 *
 * The microinstructions are an abstraction, the real cpu used a cycle counter
 * and a "Random Logic Controller" to determine how the datapath should be
 * controlled. It seemed silly to reimplement that in code, as going that low
 * level wouldn't be helpful to accuracy.
 *
 * The data and memory micro instructions can be found in data_ops.cc and
 * mem_ops.cc, respectively.
 *
 * Interrupts are polled for at the end of each cycle, and checked during the
 * last micro instruction of an instruction. Interrupts are generated using
 * the NMI and IRQ lines, which can be set by other files.
 *
 * The CPU can be suspended by executing an object attribute memory DMA,
 * which is started through a memory access to $4014.
 *
 * References to the APU, PPU, and IO are not found in this file, as they
 * are handled by MMIO and, thus, part of the memory implementation.
 */

#include "./cpu.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/header.h"
#include "../util/util.h"
#include "./machinecode.h"
#include "./cpu_state.h"
#include "./cpu_operation.h"

// DMA transfers take at least 513 cycles.
#define DMA_CYCLE_LENGTH 513U

// Used by memory operations to access both the high and low byte of an
// addressing register.
#define READ_ADDR_REG(r, o) (static_cast<DoubleWord>(\
                     (((reinterpret_cast<DataWord*>(regs_))[(r) + 1]) << 8)\
                  | ((((reinterpret_cast<DataWord*>(regs_))[(r)]) + o) & 0xFF)))

// Used to access and update the processor status register.
#define P_MASK   0xEFU
#define P_BASE   0x20U
#define P_FLAG_N 0x80U
#define P_FLAG_V 0x40U
#define P_FLAG_B 0x30U
#define P_FLAG_D 0x08U
#define P_FLAG_I 0x04U
#define P_FLAG_Z 0x02U
#define P_FLAG_C 0x01U
#define P_UPDATE_N(w) (regs_->p = (static_cast<DataWord>(w) & 0x80)\
                                | (regs_->p & 0x7F))
#define P_UPDATE_Z(w) (regs_->p = ((static_cast<DataWord>(w) == 0) << 1)\
                                | (regs_->p & 0xFD))
#define P_UPDATE_V(b) (regs_->p = ((static_cast<bool>(b)) << 6)\
                                | (regs_->p & 0xBF))
#define P_UPDATE_C(b) (regs_->p = ((static_cast<bool>(b)) | (regs_->p & 0xFE)))

/*
 * Creates a new CPU object. The CPU object will not be in a usable state
 * until Connect() and Power() have been called.
 */
Cpu::Cpu(void) {
  // Prepares the CPU's internal structures.
  state_ = new CpuState();
  regs_ = new CpuRegFile();
  return;
}

/*
 * Connects the given Memory object to the calling CPU object.
 * This memory object will then be used in the emulation.
 */
void Cpu::Connect(Memory *memory) {
  memory_ = memory;
  return;
}

/*
 * Loads the reset vector from memory and then adds a cycle to the state
 * queue. This function must be called before RunCycle() can be used.
 *
 * Assumes that the CPU has been connected to a valid Memory object.
 */
void Cpu::Power(void) {
  // Loads the reset location into the program counter.
  regs_->pc_lo = memory_->Read(MEMORY_RESET_ADDR);
  regs_->pc_hi = memory_->Read(MEMORY_RESET_ADDR + 1U);

  // Queues the first cycle to be emulated.
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Executes CPU cycles until either the given cycle limit has been reached
 * or the CPU must sync with the APU/PPU. Returns the number of cycles that
 * were executed. Fills the sync pointer with the number of cycles the CPU
 * should be synced to the APU/PPU.
 *
 * Assumes the CPU has been connected to a valid memory objected and at
 * least one cycle is in the state queue.
 */
size_t Cpu::RunSchedule(size_t cycles, size_t *syncs) {
  // Execute the CPU until it must be synced.
  size_t execs = 0;
  while (CheckNextCycle() && (execs < cycles)
                          && (dma_cycles_remaining_ == 0)) {
    RunCycle();
    execs++;
  }

  // The CPU must be synced for the duration of any DMA.
  *syncs = MAX(dma_cycles_remaining_, 1UL);
  return execs;
}

/*
 * Checks if the next memory operation is safe to perform while the CPU
 * is not synced with the PPU and APU.
 */
bool Cpu::CheckNextCycle(void) {
  // Decodes the information necessary to complete the memory operation.
  CpuOperation op = state_->PeekCycle();
  CpuReg mem_addr = GET_MEM_ADDR(op);
  DoubleWord mem_offset = GET_MEM_OFST(op);
  MemoryOpcode mem_op = GET_MEM_OP(op);

  // Checks the operation encoded by memory.
  switch (mem_op) {
    case MEM_READ:
    case MEM_READZP:
      return memory_->CheckRead(READ_ADDR_REG(mem_addr, mem_offset));
    case MEM_WRITE:
      return memory_->CheckWrite(READ_ADDR_REG(mem_addr, 0));
    case MEM_FETCH:
    case MEM_BRANCH:
      return memory_->CheckRead(READ_ADDR_REG(REG_PCL, 0));
    default:
      return true;
  }
}

/*
 * Executes the next cycle in the cpu emulation using the cpu structures.
 *
 * Assumes that there is at least one cycle in the state queue.
 */
void Cpu::RunCycle(void) {
  // Check if the CPU is suspended and executing a DMA.
  if (dma_cycles_remaining_ > 0) {
    ExecuteDma();
    cycle_even_ = !cycle_even_;
    return;
  }

  // Poll the interrupt detectors, if it is time to do so.
  if (CanPoll()) {
    // irq_ready should only be reset from a fetch call handling it
    // or from interrupts being blocked.
    irq_ready_ = (irq_ready_ || irq_level_) && !(regs_->p & P_FLAG_I);
  }

  // Fetch and run the next micro instructions for the cycle.
  RunOperation(state_->NextCycle());

  // Poll the interrupt lines and update the detectors.
  PollNmiLine();
  PollIrqLine();

  // Toggle the cycle evenness.
  cycle_even_ = !cycle_even_;

  return;
}

/*
 * Starts an OAM DMA transfer at the given address.
 */
void Cpu::StartDma(DataWord addr) {
  // Starts the dma transfer by setting the high CPU memory byte and reseting
  // the cycle counter.
  dma_addr_.w[WORD_HI] = addr;
  dma_cycles_remaining_ = DMA_CYCLE_LENGTH;

  // If the CPU is on an odd cycle, the DMA takes one cycle longer.
  if (!cycle_even_) { dma_cycles_remaining_++; }

  return;
}

/*
 * Executes a cycle of the DMA transfer.
 */
void Cpu::ExecuteDma(void) {
  // The CPU is idle until there are <= 512 dma cycles remaining.
  if ((dma_cycles_remaining_ < DMA_CYCLE_LENGTH) && !cycle_even_) {
    // Odd cycle, so we write to OAM.
    memory_->Write(PPU_OAM_ADDR, dma_mdr_);
  } else if (dma_cycles_remaining_ < DMA_CYCLE_LENGTH) {
    // Even cycle, so we read from memory.
    dma_mdr_ = memory_->Read(dma_addr_.dw);
    dma_addr_.w[WORD_LO]++;
  } else {
    dma_mdr_ = 0;
    dma_addr_.w[WORD_LO] = 0;
  }
  dma_cycles_remaining_--;

  return;
}

/*
 * Checks if the cpu should poll for interrupts on this cycle.
 */
bool Cpu::CanPoll(void) {
  // Interrupt polling (internal) happens when the cpu is about
  // to finish an instruction and said instruction is not an interrupt.
  return (state_->GetSize() == 2) && (regs_->inst != INST_BRK);
}

/*
 * Runs the Memory, Data, and PC action specified by the given operation.
 */
void Cpu::RunOperation(CpuOperation op) {
  // Performs the memory operation.
  RunMemoryOperation(op);

  // Performs the encoded data operation.
  RunDataOperation(op);

  // Increments the PC by the given value.
  DoubleWord pc_update = GET_DOUBLE_WORD(regs_->pc_lo, regs_->pc_hi)
                       + GET_PC_INC(op);
  regs_->pc_lo = GET_WORD_LO(pc_update);
  regs_->pc_hi = GET_WORD_HI(pc_update);

  return;
}

/*
 * Runs the memory operation encoded by the given CPU operation.
 */
void Cpu::RunMemoryOperation(CpuOperation &op) {
  // Decodes the information necessary to complete the memory operation.
  CpuReg mem_addr = GET_MEM_ADDR(op);
  CpuReg mem_op1 = GET_MEM_OP1(op);
  CpuReg mem_op2 = GET_MEM_OP2(op);
  (void)mem_op2; // Used for undefined instructions, which are not implemented.
  DoubleWord mem_offset = GET_MEM_OFST(op);
  MemoryOpcode mem_op = GET_MEM_OP(op);
  DataWord *regfile = reinterpret_cast<DataWord*>(regs_);

  // Performs the encoded memory operation.
  switch (mem_op) {
    /*
     * Clears the high byte of the address pointed to by mem_op1, then
     * passes off to MEM_READ to perform the read of the low address byte.
     */
    case MEM_READZP: {
      regfile[mem_op1 + 1] = 0;
      [[fallthrough]];
    }

    /*
     * Reads the value from the address specified by the addressing register
     * plus the offset into the register specified by mem_op1.
     */
    case MEM_READ: {
      regfile[mem_op1] = memory_->Read(READ_ADDR_REG(mem_addr, mem_offset));
      break;
    }

    /*
     * Writes the value specified by mem_op1 & mem_op2 into the address
     * given from the addressing register.
     */
    case MEM_WRITE: {
      memory_->Write(READ_ADDR_REG(mem_addr, 0), regfile[mem_op1]);
      break;
    }

    /*
     * Sets the B flag in P and lowers the irq_ready signal, then passes
     * off to MEM_IRQ to queue the next cycles according to hijacking behavior.
     */
    case MEM_BRK: {
      regs_->p |= P_FLAG_B;
      irq_ready_ = false;
      [[fallthrough]];
    }

    /*
     * Pushes the state register on the stack, then adds the next cycles of
     * the interrupt according to hijacking behavior. The B flag is then
     * masked out of P.
     */
    case MEM_IRQ: {
      memory_->Write(READ_ADDR_REG(REG_S, 0), regs_->p);

      // Allows an NMI to hijack the instruction.
      if (nmi_edge_) {
        state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                       | MEM_OFST(0) | DAT_SET | DAT_MASK(P_FLAG_I));
        state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                       | MEM_OFST(1));
        state_->AddCycle(MEM_FETCH | PC_INC);
      } else {
        state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                       | MEM_OFST(4) | DAT_SET | DAT_MASK(P_FLAG_I));
        state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                       | MEM_OFST(5));
        state_->AddCycle(MEM_FETCH | PC_INC);
      }

      regs_->p &= P_MASK;
      break;
    }

    /*
     * Pushes the status register onto the stack with the B flag set.
     */
    case MEM_PHP: {
      memory_->Write(READ_ADDR_REG(REG_S, 0), regs_->p | P_FLAG_B);
      break;
    }

    /*
     * Pulls the status register from the stack, clearing the B flag.
     */
    case MEM_PLP: {
      regs_->p = memory_->Read(READ_ADDR_REG(REG_S, 0)) & P_MASK;
      break;
    }

    /*
     * Fetches the next instructions and queues its cycles.
     */
    case MEM_FETCH: {
      Fetch(op);
      break;
    }

    /*
     * Executes a branch instruction.
     * Branch instructions are of the form xxy10000, and are broken into
     * three cases:
     * 1) If the flag indicated by xx has value y, then the relative address
     * is added to the PC.
     * 2) If case 1 results in a page crossing on the pc, an extra cycle is
     * added.
     * 3) If xx does not have value y, this micro op is the same as MEM_FETCH.
     */
    case MEM_BRANCH: {
      // Calculate whether or not the branch was taken.
      DataWord flag = (regs_->inst >> 6U) & 0x03U;
      bool cond = regs_->inst & 0x20U;
      // Black magic that pulls the proper flag from the status reg.
      DataWord status = regs_->p;
      flag = (flag & 2U) ? ((status >> (flag & 1U)) & 1U)
                         : ((status >> ((~flag) & 0x07U)) & 1U);
      bool taken = ((static_cast<bool>(flag)) == cond);

      // Add the reletive address to pc_lo. Reletive addressing is signed.
      DoubleWord res = regs_->pc_lo + regs_->temp1;
      // Effectively sign extends the MDR in the carry out.
      regs_->temp2 = (regs_->temp1 & 0x80) ? (GET_WORD_HI(res) + 0xFFU)
                                           :  GET_WORD_HI(res);

      // Execute the proper cycles according to the above results.
      if (!taken) {
        // Case 3. We must force a PC increment since this is now a fetch.
        op |= PC_INC;
        Fetch(op);
      } else if (regs_->temp2) {
        // Case 2.
        regs_->pc_lo = GET_WORD_LO(res);
        state_->AddCycle(DAT_ADD | DAT_SRC(REG_TMP2) | DAT_DST(REG_PCH));
        state_->AddCycle(MEM_FETCH | PC_INC);
      } else {
        // Case 1.
        regs_->pc_lo = GET_WORD_LO(res);
        state_->AddCycle(MEM_FETCH | PC_INC);
      }
      break;
    }

    /* Does nothing */
    case MEM_NOP:
    default: {
      break;
    }
  }

  return;
}

/*
 * Executes the data operation encoded by the given CPU operation.
 */
void Cpu::RunDataOperation(CpuOperation &op) {
  // Decodes the data information from the given operation.
  CpuReg data_src = GET_DAT_SRC(op);
  CpuReg data_dst = GET_DAT_DST(op);
  DataWord data_mask = GET_DAT_MASK(op);
  DataOpcode data_op = GET_DAT_OP(op);
  DataWord *regfile = reinterpret_cast<DataWord*>(regs_);

  // Performs the specified data operation.
  DoubleWord res = 0;
  switch (data_op) {
    /*
     * Increments the specified destination register.
     * Updates the N and Z flags according to the result.
     */
    case DAT_INC: {
      regfile[data_dst]++;
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Increments the specified destination register.
     */
    case DAT_INCNF: {
      regfile[data_dst]++;
      break;
    }

    /*
     * Decrements the specified destination register.
     * Updates the N and Z flags according to the result.
     */
    case DAT_DEC: {
      regfile[data_dst]--;
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Decrements the specified destination register.
     */
    case DAT_DECNF: {
      regfile[data_dst]--;
      break;
    }

    /*
     * Moves the value of the source register into the destination register.
     * Updates the N and Z flags based on the result.
     */
    case DAT_MOV: {
      regfile[data_dst] = regfile[data_src];
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Moves the value of the source register into the destination register.
     */
    case DAT_MOVNF: {
      regfile[data_dst] = regfile[data_src];
      break;
    }

    /*
     * Masks out the bits specified in the data mask from P.
     */
    case DAT_CLS: {
      regs_->p &= (~data_mask);
      break;
    }

    /*
     * Sets the bits specified in the data mask in P.
     */
    case DAT_SET: {
      regs_->p |= data_mask;
      break;
    }

    /*
     * Updates the N, Z, and C flags according to the result of (rd - rs).
     */
    case DAT_CMP: {
      P_UPDATE_N(regfile[data_dst] - regfile[data_src]);
      P_UPDATE_C(regfile[data_dst] >= regfile[data_src]);
      P_UPDATE_Z(regfile[data_dst] - regfile[data_src]);
      break;
    }

    /*
     * Shifts the destination register left once, storing the lost bit in C
     * and updating N and Z according to the result.
     */
    case DAT_ASL: {
      P_UPDATE_C(regfile[data_dst] & 0x80);
      regfile[data_dst] <<= 1;
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Shifts the destination register right once, storing the lost bit in C
     * and updating N and Z according to the result. The high bit is filled
     * with 0.
     */
    case DAT_LSR: {
      P_UPDATE_C(regfile[data_dst] & 0x01);
      regfile[data_dst] >>= 1;
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Shifts the destination register left once, back filling with C.
     * Stores the lost bit in C and sets N and Z according to the result.
     */
    case DAT_ROL: {
      res = (regfile[data_dst] << 1) | (regs_->p & P_FLAG_C);
      regfile[data_dst] = GET_WORD_LO(res);
      P_UPDATE_C(GET_WORD_HI(res));
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Shifts the destination register right once, back filling with C.
     * Stores the lost bit in C and sets N and Z according to the result.
     */
    case DAT_ROR: {
      res = GET_DOUBLE_WORD(regfile[data_dst], regs_->p);
      regfile[data_dst] = res >> 1;
      P_UPDATE_C(res & 0x01);
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * XOR's the specified registers and sets the N and Z flags
     * according to the result.
     */
    case DAT_XOR: {
      regfile[data_dst] ^= regfile[data_src];
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * OR's the specified registers and sets the N and Z flags
     * according to the result.
     */
    case DAT_OR: {
      regfile[data_dst] |= regfile[data_src];
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * AND's the specified registers and sets the N and Z flags
     * according to the result.
     */
    case DAT_AND: {
      regfile[data_dst] &= regfile[data_src];
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      break;
    }

    /*
     * Adds the specified registers, storing the carry out in tmp2.
     */
    case DAT_ADD: {
      res = regfile[data_dst] + regfile[data_src];
      regfile[data_dst] = GET_WORD_LO(res);
      regfile[REG_TMP2] = GET_WORD_HI(res);
      break;
    }

    /*
     * Nots the source register, then passes off to ADC to perform the addition
     * and set the flags. This is the same as rd = rd - rs - (1 - C).
     */
    case DAT_SBC: {
      regfile[data_src] = ~(regfile[data_src]);
      [[fallthrough]];
    }

    /*
     * Adds the source register and the carry flag to the destination register.
     * Sets the N, V, Z, and C flags according to the result.
     */
    case DAT_ADC: {
      res = regfile[data_dst] + regfile[data_src] + (regs_->p & P_FLAG_C);
      P_UPDATE_V(((~(regfile[data_dst] ^ regfile[data_src]))
                  & (regfile[data_dst] ^ GET_WORD_HI(res))) & 0x80);
      regfile[data_dst] = GET_WORD_LO(res);
      P_UPDATE_N(regfile[data_dst]);
      P_UPDATE_Z(regfile[data_dst]);
      P_UPDATE_C(GET_WORD_HI(res));
      break;
    }

    /*
     * Moves the high 2 bits of the source register to P, then sets
     * the Z flag according to the result of (rd & rs).
     */
    case DAT_BIT: {
      P_UPDATE_N(regfile[data_src]);
      P_UPDATE_V(regfile[data_src] & P_FLAG_V);
      P_UPDATE_Z(regfile[data_dst] & regfile[data_src]);
      break;
    }

    /*
     * Pushes the last addressing operation back onto the queue if the address
     * crossed a page bound and needed to be fixed.
     * The destination register is the high byte of the address, and the source
     * register is the carry out.
     */
    case DAT_VFIX: {
      if (regfile[data_src]) {
        regfile[data_dst] += regfile[data_src];
        state_->PushCycle(op & MEMORY_OPERATION_MASK);
      }
      break;
    }

    /* Does nothing. */
    case DAT_NOP:
    default: {
      break;
    }
  }

  return;
}

/*
 * Fetches the next instruction to be executing, storing brk instead
 * if an interrupt should be started. Adjusts the PC as necessary, since
 * interrupts do not increment it.
 */
void Cpu::Fetch(CpuOperation &op) {
  // Fetch the next instruction to the instruction register.
  if (!nmi_edge_ && !irq_ready_) {
    regs_->inst = memory_->Read(READ_ADDR_REG(REG_PCL, 0));
  } else {
    // All interrupts fill the instruction register with 0x00 (BRK).
    regs_->inst = INST_BRK;
    // Interrupts should not increment the PC.
    op &= (~PC_INC);
  }

  // Decode the instruction.
  DecodeInst();

  return;
}

/*
 * Decodes the next instruction (or interrupt) into micro instructions and
 * adds them to the state queue. Resets the interrupt flags if one is
 * processed.
 */
void Cpu::DecodeInst(void) {
  // We only decode the instruction if there are no interrupts.
  if (nmi_edge_) {
    // An nmi signal has priority over an irq, and resets the ready flag for it.
    nmi_edge_ = false;
    irq_ready_ = false;

    // Since an nmi was detected, we queue its cycles and return.
    state_->AddCycle(MEM_READ | MEM_ADDR(REG_PCL) | MEM_OP1(REG_TMP2));
    state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCH)
                   | DAT_DECNF | DAT_DST(REG_S));
    state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCL)
                   | DAT_DECNF | DAT_DST(REG_S));
    state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_P)
                   | DAT_DECNF | DAT_DST(REG_S));
    state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                                                  | MEM_OFST(OFFSET_NMIL)
                   | DAT_SET | DAT_MASK(P_FLAG_I));
    state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                                                  | MEM_OFST(OFFSET_NMIH));
    state_->AddCycle(MEM_FETCH | PC_INC);
    return;
  } else if (irq_ready_) {
    // The irq has been handled, so we reset the flag.
    irq_ready_ = false;

    // Since an irq was detected, we queue its cycles and return.
    state_->AddCycle(MEM_READ | MEM_ADDR(REG_PCL) | MEM_OP1(REG_TMP2));
    state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCH)
                   | DAT_DECNF | DAT_DST(REG_S));
    state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCL)
                   | DAT_DECNF | DAT_DST(REG_S));
    state_->AddCycle(MEM_IRQ | DAT_DECNF | DAT_DST(REG_S));
    return;
  }

  /*
   * Since there were no interrupts, we case on the instruction and decode it.
   *
   * The 6502 has serveral different addressing modes an instruction can use,
   * and so most instructions will simply call a helper function to decode that
   * addressing mode (with one micro op being changed to perform the desired
   * instruction).
   */
  switch (regs_->inst) {
    case INST_ORA_IZPX:
      DecodeIzpx(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ZP:
      DecodeZp(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_IMM:
      DecodeImm(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ABS:
      DecodeAbs(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_IZP_Y:
      DecodeIzpY(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ZPX:
      DecodeZpx(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ABY:
      DecodeAby(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ABX:
      DecodeAbx(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_IZPX:
      DecodeIzpx(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ZP:
      DecodeZp(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_IMM:
      DecodeImm(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ABS:
      DecodeAbs(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_IZP_Y:
      DecodeIzpY(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ZPX:
      DecodeZpx(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ABY:
      DecodeAby(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ABX:
      DecodeAbx(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_IZPX:
      DecodeIzpx(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ZP:
      DecodeZp(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_IMM:
      DecodeImm(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ABS:
      DecodeAbs(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_IZP_Y:
      DecodeIzpY(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ZPX:
      DecodeZpx(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ABY:
      DecodeAby(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ABX:
      DecodeAbx(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_IZPX:
      DecodeIzpx(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ZP:
      DecodeZp(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_IMM:
      DecodeImm(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ABS:
      DecodeAbs(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_IZP_Y:
      DecodeIzpY(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ZPX:
      DecodeZpx(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ABY:
      DecodeAby(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ABX:
      DecodeAbx(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_STA_IZPX:
      DecodeWIzpx(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_IZP_Y:
      DecodeWIzpY(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ZPX:
      DecodeWZpx(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ABY:
      DecodeWAby(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ABX:
      DecodeWAbx(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_LDA_IZPX:
      DecodeIzpx(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_IZP_Y:
      DecodeIzpY(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ZPX:
      DecodeZpx(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ABY:
      DecodeAby(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ABX:
      DecodeAbx(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_IZPX:
      DecodeIzpx(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_IZP_Y:
      DecodeIzpY(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ZPX:
      DecodeZpx(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ABY:
      DecodeAby(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ABX:
      DecodeAbx(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_IZPX:
      DecodeIzpx(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ZP:
      DecodeZp(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_IMM:
      DecodeImm(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ABS:
      DecodeAbs(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_IZP_Y:
      DecodeIzpY(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ZPX:
      DecodeZpx(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ABY:
      DecodeAby(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ABX:
      DecodeAbx(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ASL_ZP:
      DecodeRwZp(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ACC:
      DecodeNomem(DAT_ASL | DAT_DST(REG_A));
      break;
    case INST_ASL_ABS:
      DecodeRwAbs(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ZPX:
      DecodeRwZpx(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ABX:
      DecodeRwAbx(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ZP:
      DecodeRwZp(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ACC:
      DecodeNomem(DAT_ROL | DAT_DST(REG_A));
      break;
    case INST_ROL_ABS:
      DecodeRwAbs(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ZPX:
      DecodeRwZpx(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ABX:
      DecodeRwAbx(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ZP:
      DecodeRwZp(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ACC:
      DecodeNomem(DAT_LSR | DAT_DST(REG_A));
      break;
    case INST_LSR_ABS:
      DecodeRwAbs(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ZPX:
      DecodeRwZpx(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ABX:
      DecodeRwAbx(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ZP:
      DecodeRwZp(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ACC:
      DecodeNomem(DAT_ROR | DAT_DST(REG_A));
      break;
    case INST_ROR_ABS:
      DecodeRwAbs(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ZPX:
      DecodeRwZpx(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ABX:
      DecodeRwAbx(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_STX_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STX_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STX_ZPY:
      DecodeWZpy(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_LDX_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ZPY:
      DecodeZpy(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ABY:
      DecodeAby(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_DEC_ZP:
      DecodeRwZp(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ABS:
      DecodeRwAbs(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ZPX:
      DecodeRwZpx(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ABX:
      DecodeRwAbx(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ZP:
      DecodeRwZp(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ABS:
      DecodeRwAbs(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ZPX:
      DecodeRwZpx(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ABX:
      DecodeRwAbx(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_BIT_ZP:
      DecodeZp(DAT_BIT | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_BIT_ABS:
      DecodeAbs(DAT_BIT | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_JMP:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_PCL) |
                       DAT_MOVNF | DAT_DST(REG_PCL) | DAT_SRC(REG_TMP1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_JMPI:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_TMP1));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_TMP1)
                                                   | MEM_OFST(1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_STY_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STY_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STY_ZPX:
      DecodeWZpx(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_LDY_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ZPX:
      DecodeZpx(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ABX:
      DecodeAbx(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPY_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPY_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPY_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_BPL:
    case INST_BMI:
    case INST_BVC:
    case INST_BVS:
    case INST_BCC:
    case INST_BCS:
    case INST_BNE:
    case INST_BEQ:
      // The branch micro instruction handles all branches.
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_BRANCH);
      break;
    case INST_BRK:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_BRK | DAT_DECNF | DAT_DST(REG_S));
      break;
    case INST_JSR:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_NOP | DAT_NOP);
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_PCL)
                     | DAT_MOVNF | DAT_DST(REG_PCL) | DAT_SRC(REG_TMP1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_RTI:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
      state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_PLP | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_RTS:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
      state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S));
      state_->AddCycle(PC_INC);
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_PHP:
      DecodePush(MEM_PHP);
      break;
    case INST_PHA:
      DecodePush(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_S));
      break;
    case INST_PLP:
      DecodePull(MEM_PLP);
      break;
    case INST_PLA:
      DecodePull(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_S)
               | DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SEC:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_C));
      break;
    case INST_SEI:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_I));
      break;
    case INST_SED:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_D));
      break;
    case INST_CLI:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_I));
      break;
    case INST_CLC:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_C));
      break;
    case INST_CLD:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_D));
      break;
    case INST_CLV:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_V));
      break;
    case INST_DEY:
      DecodeNomem(DAT_DEC | DAT_DST(REG_Y));
      break;
    case INST_DEX:
      DecodeNomem(DAT_DEC | DAT_DST(REG_X));
      break;
    case INST_INY:
      DecodeNomem(DAT_INC | DAT_DST(REG_Y));
      break;
    case INST_INX:
      DecodeNomem(DAT_INC | DAT_DST(REG_X));
      break;
    case INST_TAY:
      DecodeNomem(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_A));
      break;
    case INST_TYA:
      DecodeNomem(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_Y));
      break;
    case INST_TXA:
      DecodeNomem(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_X));
      break;
    case INST_TXS:
      DecodeNomem(DAT_MOVNF | DAT_DST(REG_S) | DAT_SRC(REG_X));
      break;
    case INST_TAX:
      DecodeNomem(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_A));
      break;
    case INST_TSX:
      DecodeNomem(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_S));
      break;
    case INST_NOP:
      DecodeNomem(MEM_NOP | DAT_NOP);
      break;
    default:
      printf("Instruction %x is not implemented\n", regs_->inst);
      abort();
  }
  return;
}

/*
 * Queues an indexed indirect zero page read instruction using the given
 * micro operation.
 */
void Cpu::DecodeIzpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1)
                 | DAT_ADD | DAT_DST(REG_TMP1) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page read instruction using the given micro operation.
 */
void Cpu::DecodeZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a read immediate instruction using the given micro operation.
 */
void Cpu::DecodeImm(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an absolute addressed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an indirect indexed zero page read instruction using the given micro
 * operation.
 */
void Cpu::DecodeIzpY(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_VFIX | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page Y indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpy(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAby(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_VFIX | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAbx(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_VFIX | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an implied (no memory access) read instruction using the given
 * micro operation.
 */
void Cpu::DecodeNomem(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page read-write instruction using the given micro operation.
 */
void Cpu::DecodeRwZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwZpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read-write instruction using the
 * given micro operation.
 */
void Cpu::DecodeRwAbx(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an X indexed indirect zero page write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWIzpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1)
                 | DAT_ADD | DAT_DST(REG_TMP1) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a zero page write instruction using the given micro operation.
 */
void Cpu::DecodeWZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an indirect addressed Y indexed zero page write instruction using the
 * given micro operation.
 */
void Cpu::DecodeWIzpY(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an X indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a Y indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpy(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAby(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAbx(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a stack push instruction using the given micro operation.
 */
void Cpu::DecodePush(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(op | DAT_DECNF | DAT_DST(REG_S));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a stack pull instruction using the given micro operation.
 */
void Cpu::DecodePull(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Checks if the global nmi line has signaled for an interrupt using an
 * edge detector.
 *
 * When an nmi is detected, the edge signal stays high until it is handled.
 */
void Cpu::PollNmiLine(void) {
  // The internal nmi signal is rising edge sensitive, so we should only set it
  // when the previous value was false and the current value is true.
  if (nmi_line_ && !nmi_prev_) { nmi_edge_ = true; }
  nmi_prev_ = nmi_line_;
  return;
}

/*
 * Checks if the global irq line has signaled for an interrupt.
 */
void Cpu::PollIrqLine(void) {
  // The internal irq signal is a level detector that update every cycle.
  irq_level_ = (irq_line_ > 0);
  return;
}

/*
 * Frees the register file, memory, and state.
 */
Cpu::~Cpu(void) {
  delete regs_;
  delete state_;
}
