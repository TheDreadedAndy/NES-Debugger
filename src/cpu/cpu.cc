/*
 * FIXME: This description is extremely out of date.
 *
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
  code_table_ = LoadCodeTable();
  regs_ = new CpuRegFile();
  return;
}

/*
 * Loads the microcode table from the linked binary into ram, and
 * returns it to the CPU.
 */
CpuOperation *Cpu::LoadCodeTable(void) {
  extern const CpuOperation _binary_bins_inst_table_bin_start[];
  const size_t table_size = NUM_DEFINED_INSTRUCTION * kInstSequenceSize_;

  CpuOperation *table = new CpuOperation[table_size];
  for (size_t i = 0; i < table_size; i++) {
    table[i] = _binary_bins_inst_table_bin_start[i];
  }
  return table;
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
  inst_buffer_[0] = MEM_FETCH | PC_INC;
  current_sequence_ = inst_buffer_;
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
size_t Cpu::RunSchedule(size_t cycles, size_t &syncs) {
  // Execute the CPU until it must be synced.
  size_t execs = 0;
  while (CheckNextCycle() && (execs < cycles)
                          && (dma_cycles_remaining_ == 0)) {
    RunCycle();
    execs++;
  }

  // The CPU must be synced for the duration of any DMA.
  syncs = MAX(dma_cycles_remaining_, 1UL);
  return execs;
}

/*
 * Checks if the next memory operation is safe to perform while the CPU
 * is not synced with the PPU and APU.
 */
bool Cpu::CheckNextCycle(void) {
  // Decodes the information necessary to complete the memory operation.
  CpuOperation op = current_sequence_[inst_pointer_];
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
  RunOperation();

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
  return ((current_sequence_[inst_pointer_]
       & MEMORY_OPERATION_MASK) != MEM_FETCH)
      && ((current_sequence_[inst_pointer_]
       & MEMORY_OPERATION_MASK) != MEM_BRANCH)
      && (inst_pointer_ + 1 < kInstSequenceSize_)
      && (((current_sequence_[inst_pointer_ + 1]
       & MEMORY_OPERATION_MASK) == MEM_FETCH)
      || ((current_sequence_[inst_pointer_ + 1]
       & MEMORY_OPERATION_MASK) == MEM_BRANCH))
      && (regs_->inst != INST_BRK);
}

/*
 * Runs the Memory, Data, and PC action specified by the given operation.
 */
void Cpu::RunOperation(void) {
  // Pulls the next operation from the microcode sequence.
  current_operation_ = current_sequence_[inst_pointer_];
  inst_pointer_++;

  // Performs the memory operation.
  RunMemoryOperation(current_operation_);

  // Performs the encoded data operation.
  RunDataOperation(current_operation_);

  // Increments the PC by the given value.
  DoubleWord pc_update = GET_DOUBLE_WORD(regs_->pc_lo, regs_->pc_hi)
                       + GET_PC_INC(current_operation_);
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
        inst_buffer_[0] = MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                        | MEM_OFST(0) | DAT_SET | DAT_MASK(P_FLAG_I);
        inst_buffer_[1] = MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                        | MEM_OFST(1);
        inst_buffer_[2] = MEM_FETCH | PC_INC;
        current_sequence_ = inst_buffer_;
        inst_pointer_ = 0;
      } else {
        inst_buffer_[0] = MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                        | MEM_OFST(4) | DAT_SET | DAT_MASK(P_FLAG_I);
        inst_buffer_[1] = MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                        | MEM_OFST(5);
        inst_buffer_[2] = MEM_FETCH | PC_INC;
        current_sequence_ = inst_buffer_;
        inst_pointer_ = 0;
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
        inst_buffer_[0] = DAT_ADD | DAT_SRC(REG_TMP2) | DAT_DST(REG_PCH);
        inst_buffer_[1] = MEM_FETCH | PC_INC;
        current_sequence_ = inst_buffer_;
        inst_pointer_ = 0;
      } else {
        // Case 1.
        regs_->pc_lo = GET_WORD_LO(res);
        inst_buffer_[0] = MEM_FETCH | PC_INC;
        current_sequence_ = inst_buffer_;
        inst_pointer_ = 0;
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
        inst_buffer_[0] = op & MEMORY_OPERATION_MASK;
        inst_buffer_[1] = current_sequence_[inst_pointer_];
        current_sequence_ = inst_buffer_;
        inst_pointer_ = 0;
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
    // Read the inst from the PC, then decode it using the code table.
    regs_->inst = memory_->Read(READ_ADDR_REG(REG_PCL, 0));
    current_sequence_ = &code_table_[regs_->inst * kInstSequenceSize_];

    // Ensure that the instruction loaded was not illegal.
    // In the code table, no valid instruction begins with a NOP.
    if (current_sequence_[0] == 0) {
      fprintf(stderr, "Error: Fetch obtained unimplemented instruction %02x",
              regs_->inst);
      abort();
    }
  } else {
    // All interrupts fill the instruction register with 0x00 (BRK).
    regs_->inst = INST_BRK;
    // Interrupts should not increment the PC.
    op &= (~PC_INC);

    // Decode the interrupt from the code table.
    if (nmi_edge_) {
      nmi_edge_ = false;
      irq_ready_ = false;
      current_sequence_ = &code_table_[EINST_NMI * kInstSequenceSize_];
    } else {
      irq_ready_ = false;
      current_sequence_ = &code_table_[EINST_IRQ * kInstSequenceSize_];
    }
  }

  // Reset the index in the microcode sequence.
  inst_pointer_ = 0;

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
  delete[] code_table_;
}
