/*
 * This file contains the interpreter for the emulation of the NES's 2A03 6502
 * CPU.
 *
 * Instructions are run using a state queue, found in state.c, which is filled
 * by the fetch phase of an instruction. The queue contains micro instructions
 * for data and memory operations, which are executed in pairs of memory and
 * data each cycle.
 *
 * The microinstructions are an abstraction, the real cpu used a cycle counter
 * and an RLC to determine how the datapath should be controlled. It seemed
 * silly to reimplement that in code, as going that low level wouldn't be
 * helpful to accuracy.
 *
 * The data and memory micro instructions can be found in microdata.c and
 * micromem.c, respectively.
 *
 * Interrupts are polled for at the end of each cycle, and checked during the
 * last micro instruction of an instruction. Interrupts are generated using
 * the NMI and IRQ lines, which can be set by other files.
 *
 * The CPU can be suspended by executing an object attribute memory DMA,
 * which is started through a memory access to $4014.
 *
 * References to the APU, PPU, and IO are not found in this file, as they
 * are handled by MMIO and, thus, part of memory.c.
 */

#include "./cpu.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/header.h"
#include "./machinecode.h"
#include "./cpu_status.h"
#include "./cpu_state.h"

// DMA transfers take at least 513 cycles.
#define DMA_CYCLE_LENGTH 513U

/*
 * Initializes everything related to the cpu so that the emulation can begin.
 */
Cpu::Cpu(Memory *memory) {
  // Init all cpu structures.
  memory_ = memory;
  state_ = new CpuState;
  regs_ = new CpuRegFile;

  // Setup the cpu register file.
  // On startup, IRQ's are disabled and the high byte of the stack pointer
  // is set to 0x01.
  regs_->p.irq_disable = true;
  regs_->s.w[WORD_HI] = MEMORY_STACK_HIGH;

  // Load the reset location into the pc.
  R->pc.w[WORD_LO] = memory_->Read(MEMORY_RESET_ADDR);
  R->pc.w[WORD_HI] = memory_->Read(MEMORY_RESET_ADDR + 1);

  // Queue the first cycle to be emulated.
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Executes the next cycle in the cpu emulation using the cpu structures.
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
    irq_ready_ = (irq_ready_ || irq_level_) && !(regs_->p.irq_disable);
  }

  // Fetch and run the next micro instructions for the cycle.
  OperationCycle *next_op = state_->NextCycle();
  next_op->mem();
  next_op->data();
  if (next_op->inc_pc) { regs_->pc.dw++; }

  // Poll the interrupt lines and update the detectors.
  PollNmiLine();
  PollIrqLine();

  // Toggle the cycle evenness.
  cycle_even_ = !cycle_even_;

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
 * Starts an OAM DMA transfer at the given address.
 */
void Cpu::StartDma(word_t addr) {
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
 * Fetches the next instruction to be executing, storing brk instead
 * if an interrupt should be started. Adjusts the PC as necessary, since
 * branch calls which lead into a fetch do not.
 *
 * This functions should only be called from MemFetch().
 */
void Cpu::Fetch(OperationCycle *op) {
  // Fetch the next instruction to the instruction register.
  if (!nmi_edge_ && !irq_ready_) {
    // A non-interrupt fetch should always be paired with a PC increment.
    // We set the micro ops pc_inc field here, in case we were coming
    // from a branch.
    op->inc_pc = PC_INC;
    regs_->inst = memory_->Read(regs_->pc.dw);
  } else {
    // All interrupts fill the instruction register with 0x00 (BRK).
    regs_->inst = INST_BRK;
    // Interrupts should not increment the PC.
    op->inc_pc = PC_NOP;
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
    state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
    state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
    state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
    state_->AddCycle(&MemPushP, &DataDecS, PC_NOP);
    state_->AddCycle(&MemNmiPcl, &DataSei, PC_NOP);
    state_->AddCycle(&MemNmiPch, &Nop, PC_NOP);
    state_->AddCycle(&MemFetch, &Nop, PC_INC);
    return;
  } else if (irq_ready_) {
    // The irq has been handled, so we reset the flag.
    irq_ready_ = false;

    // Since an irq was detected, we queue its cycles and return.
    state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
    state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
    state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
    state_->AddCycle(&MemIrq, &DataDecS, PC_NOP);
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
      DecodeIzpx(&DataOraMdrA);
      break;
    case INST_ORA_ZP:
      DecodeZp(&DataOraMdrA);
      break;
    case INST_ORA_IMM:
      DecodeImm(&DataOraMdrA);
      break;
    case INST_ORA_ABS:
      DecodeAbs(&DataOraMdrA);
      break;
    case INST_ORA_IZP_Y:
      DecodeIzpY(&DataOraMdrA);
      break;
    case INST_ORA_ZPX:
      DecodeZpx(&DataOraMdrA);
      break;
    case INST_ORA_ABY:
      DecodeAby(&DataOraMdrA);
      break;
    case INST_ORA_ABX:
      DecodeAbx(&DataOraMdrA);
      break;
    case INST_AND_IZPX:
      DecodeIzpx(&DataAndMdrA);
      break;
    case INST_AND_ZP:
      DecodeZp(&DataAndMdrA);
      break;
    case INST_AND_IMM:
      DecodeImm(&DataAndMdrA);
      break;
    case INST_AND_ABS:
      DecodeAbs(&DataAndMdrA);
      break;
    case INST_AND_IZP_Y:
      DecodeIzpY(&DataAndMdrA);
      break;
    case INST_AND_ZPX:
      DecodeZpx(&DataAndMdrA);
      break;
    case INST_AND_ABY:
      DecodeAby(&DataAndMdrA);
      break;
    case INST_AND_ABX:
      DecodeAbx(&DataAndMdrA);
      break;
    case INST_EOR_IZPX:
      DecodeIzpx(&DataEorMdrA);
      break;
    case INST_EOR_ZP:
      DecodeZp(&DataEorMdrA);
      break;
    case INST_EOR_IMM:
      DecodeImm(&DataEorMdrA);
      break;
    case INST_EOR_ABS:
      DecodeAbs(&DataEorMdrA);
      break;
    case INST_EOR_IZP_Y:
      DecodeIzpY(&DataEorMdrA);
      break;
    case INST_EOR_ZPX:
      DecodeZpx(&DataEorMdrA);
      break;
    case INST_EOR_ABY:
      DecodeAby(&DataEorMdrA);
      break;
    case INST_EOR_ABX:
      DecodeAbx(&DataEorMdrA);
      break;
    case INST_ADC_IZPX:
      DecodeIzpx(&DataAdcMdrA);
      break;
    case INST_ADC_ZP:
      DecodeZp(&DataAdcMdrA);
      break;
    case INST_ADC_IMM:
      DecodeImm(&DataAdcMdrA);
      break;
    case INST_ADC_ABS:
      DecodeAbs(&DataAdcMdrA);
      break;
    case INST_ADC_IZP_Y:
      DecodeIzpY(&DataAdcMdrA);
      break;
    case INST_ADC_ZPX:
      DecodeZpx(&DataAdcMdrA);
      break;
    case INST_ADC_ABY:
      DecodeAby(&DataAdcMdrA);
      break;
    case INST_ADC_ABX:
      DecodeAbx(&DataAdcMdrA);
      break;
    case INST_STA_IZPX:
      DecodeWIzpx(&MemWriteAAddr);
      break;
    case INST_STA_ZP:
      DecodeWZp(&MemWriteAAddr);
      break;
    case INST_STA_ABS:
      DecodeWAbs(&MemWriteAAddr);
      break;
    case INST_STA_IZP_Y:
      DecodeWIzpY(&MemWriteAAddr);
      break;
    case INST_STA_ZPX:
      DecodeWZpx(&MemWriteAAddr);
      break;
    case INST_STA_ABY:
      DecodeWAby(&MemWriteAAddr);
      break;
    case INST_STA_ABX:
      DecodeWAbx(&MemWriteAAddr);
      break;
    case INST_LDA_IZPX:
      DecodeIzpx(&DataMovMdrA);
      break;
    case INST_LDA_ZP:
      DecodeZp(&DataMovMdrA);
      break;
    case INST_LDA_IMM:
      DecodeImm(&DataMovMdrA);
      break;
    case INST_LDA_ABS:
      DecodeAbs(&DataMovMdrA);
      break;
    case INST_LDA_IZP_Y:
      DecodeIzpY(&DataMovMdrA);
      break;
    case INST_LDA_ZPX:
      DecodeZpx(&DataMovMdrA);
      break;
    case INST_LDA_ABY:
      DecodeAby(&DataMovMdrA);
      break;
    case INST_LDA_ABX:
      DecodeAbx(&DataMovMdrA);
      break;
    case INST_CMP_IZPX:
      DecodeIzpx(&DataCmpMdrA);
      break;
    case INST_CMP_ZP:
      DecodeZp(&DataCmpMdrA);
      break;
    case INST_CMP_IMM:
      DecodeImm(&DataCmpMdrA);
      break;
    case INST_CMP_ABS:
      DecodeAbs(&DataCmpMdrA);
      break;
    case INST_CMP_IZP_Y:
      DecodeIzpY(&DataCmpMdrA);
      break;
    case INST_CMP_ZPX:
      DecodeZpx(&DataCmpMdrA);
      break;
    case INST_CMP_ABY:
      DecodeAby(&DataCmpMdrA);
      break;
    case INST_CMP_ABX:
      DecodeAbx(&DataCmpMdrA);
      break;
    case INST_SBC_IZPX:
      DecodeIzpx(&DataSbcMdrA);
      break;
    case INST_SBC_ZP:
      DecodeZp(&DataSbcMdrA);
      break;
    case INST_SBC_IMM:
      DecodeImm(&DataSbcMdrA);
      break;
    case INST_SBC_ABS:
      DecodeAbs(&DataSbcMdrA);
      break;
    case INST_SBC_IZP_Y:
      DecodeIzpY(&DataSbcMdrA);
      break;
    case INST_SBC_ZPX:
      DecodeZpx(&DataSbcMdrA);
      break;
    case INST_SBC_ABY:
      DecodeAby(&DataSbcMdrA);
      break;
    case INST_SBC_ABX:
      DecodeAbx(&DataSbcMdrA);
      break;
    case INST_ASL_ZP:
      DecodeRwZp(&DataAslMdr);
      break;
    case INST_ASL_ACC:
      DecodeNomem(&DataAslA);
      break;
    case INST_ASL_ABS:
      DecodeRwAbs(&DataAslMdr);
      break;
    case INST_ASL_ZPX:
      DecodeRwZpx(&DataAslMdr);
      break;
    case INST_ASL_ABX:
      DecodeRwAbx(&DataAslMdr);
      break;
    case INST_ROL_ZP:
      DecodeRwZp(&DataRolMdr);
      break;
    case INST_ROL_ACC:
      DecodeNomem(&DataRolA);
      break;
    case INST_ROL_ABS:
      DecodeRwAbs(&DataRolMdr);
      break;
    case INST_ROL_ZPX:
      DecodeRwZpx(&DataRolMdr);
      break;
    case INST_ROL_ABX:
      DecodeRwAbx(&DataRolMdr);
      break;
    case INST_LSR_ZP:
      DecodeRwZp(&DataLsrMdr);
      break;
    case INST_LSR_ACC:
      DecodeNomem(&DataLsrA);
      break;
    case INST_LSR_ABS:
      DecodeRwAbs(&DataLsrMdr);
      break;
    case INST_LSR_ZPX:
      DecodeRwZpx(&DataLsrMdr);
      break;
    case INST_LSR_ABX:
      DecodeRwAbx(&DataLsrMdr);
      break;
    case INST_ROR_ZP:
      DecodeRwZp(&DataRorMdr);
      break;
    case INST_ROR_ACC:
      DecodeNomem(&DataRorA);
      break;
    case INST_ROR_ABS:
      DecodeRwAbs(&DataRorMdr);
      break;
    case INST_ROR_ZPX:
      DecodeRwZpx(&DataRorMdr);
      break;
    case INST_ROR_ABX:
      DecodeRwAbx(&DataRorMdr);
      break;
    case INST_STX_ZP:
      DecodeWZp(&MemWriteXAddr);
      break;
    case INST_STX_ABS:
      DecodeWAbs(&MemWriteXAddr);
      break;
    case INST_STX_ZPY:
      DecodeWZpy(&MemWriteXAddr);
      break;
    case INST_LDX_IMM:
      DecodeImm(&DataMovMdrX);
      break;
    case INST_LDX_ZP:
      DecodeZp(&DataMovMdrX);
      break;
    case INST_LDX_ABS:
      DecodeAbs(&DataMovMdrX);
      break;
    case INST_LDX_ZPY:
      DecodeZpy(&DataMovMdrX);
      break;
    case INST_LDX_ABY:
      DecodeAby(&DataMovMdrX);
      break;
    case INST_DEC_ZP:
      DecodeRwZp(&DataDecMdr);
      break;
    case INST_DEC_ABS:
      DecodeRwAbs(&DataDecMdr);
      break;
    case INST_DEC_ZPX:
      DecodeRwZpx(&DataDecMdr);
      break;
    case INST_DEC_ABX:
      DecodeRwAbx(&DataDecMdr);
      break;
    case INST_INC_ZP:
      DecodeRwZp(&DataIncMdr);
      break;
    case INST_INC_ABS:
      DecodeRwAbs(&DataIncMdr);
      break;
    case INST_INC_ZPX:
      DecodeRwZpx(&DataIncMdr);
      break;
    case INST_INC_ABX:
      DecodeRwAbx(&DataIncMdr);
      break;
    case INST_BIT_ZP:
      DecodeZp(&DataBitMdrA);
      break;
    case INST_BIT_ABS:
      DecodeAbs(&DataBitMdrA);
      break;
    case INST_JMP:
      state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
      state_->AddCycle(&MemReadPcPch, &DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_JMPI:
      state_->AddCycle(&MemReadPcPtrl, &Nop, PC_INC);
      state_->AddCycle(&MemReadPcPtrh, &Nop, PC_INC);
      state_->AddCycle(&MemReadPtrMdr, &Nop, PC_NOP);
      state_->AddCycle(&MemReadPtr1Pch, &DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_STY_ZP:
      DecodeWZp(&MemWriteYAddr);
      break;
    case INST_STY_ABS:
      DecodeWAbs(&MemWriteYAddr);
      break;
    case INST_STY_ZPX:
      DecodeWZpx(&MemWriteYAddr);
      break;
    case INST_LDY_IMM:
      DecodeImm(&DataMovMdrY);
      break;
    case INST_LDY_ZP:
      DecodeZp(&DataMovMdrY);
      break;
    case INST_LDY_ABS:
      DecodeAbs(&DataMovMdrY);
      break;
    case INST_LDY_ZPX:
      DecodeZpx(&DataMovMdrY);
      break;
    case INST_LDY_ABX:
      DecodeAbx(&DataMovMdrY);
      break;
    case INST_CPY_IMM:
      DecodeImm(&DataCmpMdrY);
      break;
    case INST_CPY_ZP:
      DecodeZp(&DataCmpMdrY);
      break;
    case INST_CPY_ABS:
      DecodeAbs(&DataCmpMdrY);
      break;
    case INST_CPX_IMM:
      DecodeImm(&DataCmpMdrX);
      break;
    case INST_CPX_ZP:
      DecodeZp(&DataCmpMdrX);
      break;
    case INST_CPX_ABS:
      DecodeAbs(&DataCmpMdrX);
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
      state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
      state_->AddCycle(&Nop, &DataBranch, PC_NOP);
      break;
    case INST_BRK:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_INC);
      state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
      state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
      state_->AddCycle(&MemBrk, &DataDecS, PC_NOP);
      break;
    case INST_JSR:
      state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
      state_->AddCycle(&Nop, &Nop, PC_NOP);
      state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
      state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
      state_->AddCycle(&MemReadPcPch, &DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_RTI:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &DataIncS, PC_NOP);
      state_->AddCycle(&MemPullP, &DataIncS, PC_NOP);
      state_->AddCycle(&MemPullPcl, &DataIncS, PC_NOP);
      state_->AddCycle(&MemPullPch, &Nop, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_RTS:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &DataIncS, PC_NOP);
      state_->AddCycle(&MemPullPcl, &DataIncS, PC_NOP);
      state_->AddCycle(&MemPullPch, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &Nop, PC_INC);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_PHP:
      DecodePush(&MemPushPB);
      break;
    case INST_PHA:
      DecodePush(&MemPushA);
      break;
    case INST_PLP:
      DecodePull(&MemPullP);
      break;
    case INST_PLA:
      DecodePull(&MemPullA);
      break;
    case INST_SEC:
      DecodeNomem(&DataSec);
      break;
    case INST_SEI:
      DecodeNomem(&DataSei);
      break;
    case INST_SED:
      DecodeNomem(&DataSed);
      break;
    case INST_CLI:
      DecodeNomem(&DataCli);
      break;
    case INST_CLC:
      DecodeNomem(&DataClc);
      break;
    case INST_CLD:
      DecodeNomem(&DataCld);
      break;
    case INST_CLV:
      DecodeNomem(&DataClv);
      break;
    case INST_DEY:
      DecodeNomem(&DataDecY);
      break;
    case INST_DEX:
      DecodeNomem(&DataDecX);
      break;
    case INST_INY:
      DecodeNomem(&DataIncY);
      break;
    case INST_INX:
      DecodeNomem(&DataIncX);
      break;
    case INST_TAY:
      DecodeNomem(&DataMovAY);
      break;
    case INST_TYA:
      DecodeNomem(&DataMovYA);
      break;
    case INST_TXA:
      DecodeNomem(&DataMovXA);
      break;
    case INST_TXS:
      DecodeNomem(&DataMovXS);
      break;
    case INST_TAX:
      DecodeNomem(&DataMovAX);
      break;
    case INST_TSX:
      DecodeNomem(&DataMovSX);
      break;
    case INST_NOP:
      DecodeNomem(&Nop);
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
void Cpu::DecodeIzpx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpPtr, &Nop, PC_INC);
  state_->AddCycle(&MemReadPtrAddrl, &DataAddPtrlX, PC_NOP);
  state_->AddCycle(&MemReadPtrAddrl, &Nop, PC_NOP);
  state_->AddCycle(&MemReadPtr1Addrh, &Nop, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page read instruction using the given micro operation.
 */
void Cpu::DecodeZp(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a read immediate instruction using the given micro operation.
 */
void Cpu::DecodeImm(CpuOperation *op) {
  state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeAbs(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an indirect indexed zero page read instruction using the given micro
 * operation.
 */
void Cpu::DecodeIzpY(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpPtr, &Nop, PC_INC);
  state_->AddCycle(&MemReadPtrAddrl, &Nop, PC_NOP);
  state_->AddCycle(&MemReadPtr1Addrh, &DataAddAddrlY, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &DataFixaAddrh, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page Y indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpy(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlY, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAby(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlY, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixaAddrh, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAbx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixaAddrh, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an implied (no memory access) read instruction using the given
 * micro operation.
 */
void Cpu::DecodeNomem(CpuOperation *op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page read-write instruction using the given micro operation.
 */
void Cpu::DecodeRwZp(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwAbs(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwZpx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read-write instruction using the
 * given micro operation.
 */
void Cpu::DecodeRwAbx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&MemWriteMdrAddr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed indirect zero page write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWIzpx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpPtr, &Nop, PC_INC);
  state_->AddCycle(&MemReadPtrAddrl, &DataAddPtrlX, PC_NOP);
  state_->AddCycle(&MemReadPtrAddrl, &Nop, PC_NOP);
  state_->AddCycle(&MemReadPtr1Addrh, &Nop, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a zero page write instruction using the given micro operation.
 */
void Cpu::DecodeWZp(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWAbs(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an indirect addressed Y indexed zero page write instruction using the
 * given micro operation.
 */
void Cpu::DecodeWIzpY(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpPtr, &Nop, PC_INC);
  state_->AddCycle(&MemReadPtrAddrl, &Nop, PC_NOP);
  state_->AddCycle(&MemReadPtr1Addrh, &DataAddAddrlY, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a Y indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpy(CpuOperation *op) {
  state_->AddCycle(&MemReadPcZpAddr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlY, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAby(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlY, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAbx(CpuOperation *op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a stack push instruction using the given micro operation.
 */
void Cpu::DecodePush(CpuOperation *op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(op, &DataDecS, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a stack pull instruction using the given micro operation.
 */
void Cpu::DecodePull(CpuOperation *op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(&Nop, &DataIncS, PC_NOP);
  state_->AddCycle(op, &Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
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
