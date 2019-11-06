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
#include <functional>
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
  regs_->pc.w[WORD_LO] = memory_->Read(MEMORY_RESET_ADDR);
  regs_->pc.w[WORD_HI] = memory_->Read(MEMORY_RESET_ADDR + 1);

  // Queue the first cycle to be emulated.
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
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
  (this->*(next_op->mem))();
  (this->*(next_op->data))();
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
    state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemPushPch, &Cpu::DataDecS, PC_NOP);
    state_->AddCycle(&Cpu::MemPushPcl, &Cpu::DataDecS, PC_NOP);
    state_->AddCycle(&Cpu::MemPushP, &Cpu::DataDecS, PC_NOP);
    state_->AddCycle(&Cpu::MemNmiPcl, &Cpu::DataSei, PC_NOP);
    state_->AddCycle(&Cpu::MemNmiPch, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
    return;
  } else if (irq_ready_) {
    // The irq has been handled, so we reset the flag.
    irq_ready_ = false;

    // Since an irq was detected, we queue its cycles and return.
    state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemPushPch, &Cpu::DataDecS, PC_NOP);
    state_->AddCycle(&Cpu::MemPushPcl, &Cpu::DataDecS, PC_NOP);
    state_->AddCycle(&Cpu::MemIrq, &Cpu::DataDecS, PC_NOP);
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
      DecodeIzpx(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_ZP:
      DecodeZp(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_IMM:
      DecodeImm(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_ABS:
      DecodeAbs(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_IZP_Y:
      DecodeIzpY(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_ZPX:
      DecodeZpx(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_ABY:
      DecodeAby(&Cpu::DataOraMdrA);
      break;
    case INST_ORA_ABX:
      DecodeAbx(&Cpu::DataOraMdrA);
      break;
    case INST_AND_IZPX:
      DecodeIzpx(&Cpu::DataAndMdrA);
      break;
    case INST_AND_ZP:
      DecodeZp(&Cpu::DataAndMdrA);
      break;
    case INST_AND_IMM:
      DecodeImm(&Cpu::DataAndMdrA);
      break;
    case INST_AND_ABS:
      DecodeAbs(&Cpu::DataAndMdrA);
      break;
    case INST_AND_IZP_Y:
      DecodeIzpY(&Cpu::DataAndMdrA);
      break;
    case INST_AND_ZPX:
      DecodeZpx(&Cpu::DataAndMdrA);
      break;
    case INST_AND_ABY:
      DecodeAby(&Cpu::DataAndMdrA);
      break;
    case INST_AND_ABX:
      DecodeAbx(&Cpu::DataAndMdrA);
      break;
    case INST_EOR_IZPX:
      DecodeIzpx(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_ZP:
      DecodeZp(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_IMM:
      DecodeImm(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_ABS:
      DecodeAbs(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_IZP_Y:
      DecodeIzpY(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_ZPX:
      DecodeZpx(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_ABY:
      DecodeAby(&Cpu::DataEorMdrA);
      break;
    case INST_EOR_ABX:
      DecodeAbx(&Cpu::DataEorMdrA);
      break;
    case INST_ADC_IZPX:
      DecodeIzpx(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_ZP:
      DecodeZp(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_IMM:
      DecodeImm(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_ABS:
      DecodeAbs(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_IZP_Y:
      DecodeIzpY(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_ZPX:
      DecodeZpx(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_ABY:
      DecodeAby(&Cpu::DataAdcMdrA);
      break;
    case INST_ADC_ABX:
      DecodeAbx(&Cpu::DataAdcMdrA);
      break;
    case INST_STA_IZPX:
      DecodeWIzpx(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_ZP:
      DecodeWZp(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_ABS:
      DecodeWAbs(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_IZP_Y:
      DecodeWIzpY(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_ZPX:
      DecodeWZpx(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_ABY:
      DecodeWAby(&Cpu::MemWriteAAddr);
      break;
    case INST_STA_ABX:
      DecodeWAbx(&Cpu::MemWriteAAddr);
      break;
    case INST_LDA_IZPX:
      DecodeIzpx(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_ZP:
      DecodeZp(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_IMM:
      DecodeImm(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_ABS:
      DecodeAbs(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_IZP_Y:
      DecodeIzpY(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_ZPX:
      DecodeZpx(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_ABY:
      DecodeAby(&Cpu::DataMovMdrA);
      break;
    case INST_LDA_ABX:
      DecodeAbx(&Cpu::DataMovMdrA);
      break;
    case INST_CMP_IZPX:
      DecodeIzpx(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_ZP:
      DecodeZp(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_IMM:
      DecodeImm(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_ABS:
      DecodeAbs(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_IZP_Y:
      DecodeIzpY(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_ZPX:
      DecodeZpx(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_ABY:
      DecodeAby(&Cpu::DataCmpMdrA);
      break;
    case INST_CMP_ABX:
      DecodeAbx(&Cpu::DataCmpMdrA);
      break;
    case INST_SBC_IZPX:
      DecodeIzpx(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_ZP:
      DecodeZp(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_IMM:
      DecodeImm(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_ABS:
      DecodeAbs(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_IZP_Y:
      DecodeIzpY(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_ZPX:
      DecodeZpx(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_ABY:
      DecodeAby(&Cpu::DataSbcMdrA);
      break;
    case INST_SBC_ABX:
      DecodeAbx(&Cpu::DataSbcMdrA);
      break;
    case INST_ASL_ZP:
      DecodeRwZp(&Cpu::DataAslMdr);
      break;
    case INST_ASL_ACC:
      DecodeNomem(&Cpu::DataAslA);
      break;
    case INST_ASL_ABS:
      DecodeRwAbs(&Cpu::DataAslMdr);
      break;
    case INST_ASL_ZPX:
      DecodeRwZpx(&Cpu::DataAslMdr);
      break;
    case INST_ASL_ABX:
      DecodeRwAbx(&Cpu::DataAslMdr);
      break;
    case INST_ROL_ZP:
      DecodeRwZp(&Cpu::DataRolMdr);
      break;
    case INST_ROL_ACC:
      DecodeNomem(&Cpu::DataRolA);
      break;
    case INST_ROL_ABS:
      DecodeRwAbs(&Cpu::DataRolMdr);
      break;
    case INST_ROL_ZPX:
      DecodeRwZpx(&Cpu::DataRolMdr);
      break;
    case INST_ROL_ABX:
      DecodeRwAbx(&Cpu::DataRolMdr);
      break;
    case INST_LSR_ZP:
      DecodeRwZp(&Cpu::DataLsrMdr);
      break;
    case INST_LSR_ACC:
      DecodeNomem(&Cpu::DataLsrA);
      break;
    case INST_LSR_ABS:
      DecodeRwAbs(&Cpu::DataLsrMdr);
      break;
    case INST_LSR_ZPX:
      DecodeRwZpx(&Cpu::DataLsrMdr);
      break;
    case INST_LSR_ABX:
      DecodeRwAbx(&Cpu::DataLsrMdr);
      break;
    case INST_ROR_ZP:
      DecodeRwZp(&Cpu::DataRorMdr);
      break;
    case INST_ROR_ACC:
      DecodeNomem(&Cpu::DataRorA);
      break;
    case INST_ROR_ABS:
      DecodeRwAbs(&Cpu::DataRorMdr);
      break;
    case INST_ROR_ZPX:
      DecodeRwZpx(&Cpu::DataRorMdr);
      break;
    case INST_ROR_ABX:
      DecodeRwAbx(&Cpu::DataRorMdr);
      break;
    case INST_STX_ZP:
      DecodeWZp(&Cpu::MemWriteXAddr);
      break;
    case INST_STX_ABS:
      DecodeWAbs(&Cpu::MemWriteXAddr);
      break;
    case INST_STX_ZPY:
      DecodeWZpy(&Cpu::MemWriteXAddr);
      break;
    case INST_LDX_IMM:
      DecodeImm(&Cpu::DataMovMdrX);
      break;
    case INST_LDX_ZP:
      DecodeZp(&Cpu::DataMovMdrX);
      break;
    case INST_LDX_ABS:
      DecodeAbs(&Cpu::DataMovMdrX);
      break;
    case INST_LDX_ZPY:
      DecodeZpy(&Cpu::DataMovMdrX);
      break;
    case INST_LDX_ABY:
      DecodeAby(&Cpu::DataMovMdrX);
      break;
    case INST_DEC_ZP:
      DecodeRwZp(&Cpu::DataDecMdr);
      break;
    case INST_DEC_ABS:
      DecodeRwAbs(&Cpu::DataDecMdr);
      break;
    case INST_DEC_ZPX:
      DecodeRwZpx(&Cpu::DataDecMdr);
      break;
    case INST_DEC_ABX:
      DecodeRwAbx(&Cpu::DataDecMdr);
      break;
    case INST_INC_ZP:
      DecodeRwZp(&Cpu::DataIncMdr);
      break;
    case INST_INC_ABS:
      DecodeRwAbs(&Cpu::DataIncMdr);
      break;
    case INST_INC_ZPX:
      DecodeRwZpx(&Cpu::DataIncMdr);
      break;
    case INST_INC_ABX:
      DecodeRwAbx(&Cpu::DataIncMdr);
      break;
    case INST_BIT_ZP:
      DecodeZp(&Cpu::DataBitMdrA);
      break;
    case INST_BIT_ABS:
      DecodeAbs(&Cpu::DataBitMdrA);
      break;
    case INST_JMP:
      state_->AddCycle(&Cpu::MemReadPcMdr, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::MemReadPcPch, &Cpu::DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
      break;
    case INST_JMPI:
      state_->AddCycle(&Cpu::MemReadPcPtrl, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::MemReadPcPtrh, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::MemReadPtrMdr, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::MemReadPtr1Pch, &Cpu::DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
      break;
    case INST_STY_ZP:
      DecodeWZp(&Cpu::MemWriteYAddr);
      break;
    case INST_STY_ABS:
      DecodeWAbs(&Cpu::MemWriteYAddr);
      break;
    case INST_STY_ZPX:
      DecodeWZpx(&Cpu::MemWriteYAddr);
      break;
    case INST_LDY_IMM:
      DecodeImm(&Cpu::DataMovMdrY);
      break;
    case INST_LDY_ZP:
      DecodeZp(&Cpu::DataMovMdrY);
      break;
    case INST_LDY_ABS:
      DecodeAbs(&Cpu::DataMovMdrY);
      break;
    case INST_LDY_ZPX:
      DecodeZpx(&Cpu::DataMovMdrY);
      break;
    case INST_LDY_ABX:
      DecodeAbx(&Cpu::DataMovMdrY);
      break;
    case INST_CPY_IMM:
      DecodeImm(&Cpu::DataCmpMdrY);
      break;
    case INST_CPY_ZP:
      DecodeZp(&Cpu::DataCmpMdrY);
      break;
    case INST_CPY_ABS:
      DecodeAbs(&Cpu::DataCmpMdrY);
      break;
    case INST_CPX_IMM:
      DecodeImm(&Cpu::DataCmpMdrX);
      break;
    case INST_CPX_ZP:
      DecodeZp(&Cpu::DataCmpMdrX);
      break;
    case INST_CPX_ABS:
      DecodeAbs(&Cpu::DataCmpMdrX);
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
      state_->AddCycle(&Cpu::MemReadPcMdr, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::Nop, &Cpu::DataBranch, PC_NOP);
      break;
    case INST_BRK:
      state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::MemPushPch, &Cpu::DataDecS, PC_NOP);
      state_->AddCycle(&Cpu::MemPushPcl, &Cpu::DataDecS, PC_NOP);
      state_->AddCycle(&Cpu::MemBrk, &Cpu::DataDecS, PC_NOP);
      break;
    case INST_JSR:
      state_->AddCycle(&Cpu::MemReadPcMdr, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::Nop, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::MemPushPch, &Cpu::DataDecS, PC_NOP);
      state_->AddCycle(&Cpu::MemPushPcl, &Cpu::DataDecS, PC_NOP);
      state_->AddCycle(&Cpu::MemReadPcPch, &Cpu::DataMovMdrPcl, PC_NOP);
      state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
      break;
    case INST_RTI:
      state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::Nop, &Cpu::DataIncS, PC_NOP);
      state_->AddCycle(&Cpu::MemPullP, &Cpu::DataIncS, PC_NOP);
      state_->AddCycle(&Cpu::MemPullPcl, &Cpu::DataIncS, PC_NOP);
      state_->AddCycle(&Cpu::MemPullPch, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
      break;
    case INST_RTS:
      state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::Nop, &Cpu::DataIncS, PC_NOP);
      state_->AddCycle(&Cpu::MemPullPcl, &Cpu::DataIncS, PC_NOP);
      state_->AddCycle(&Cpu::MemPullPch, &Cpu::Nop, PC_NOP);
      state_->AddCycle(&Cpu::Nop, &Cpu::Nop, PC_INC);
      state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
      break;
    case INST_PHP:
      DecodePush(&Cpu::MemPushPB);
      break;
    case INST_PHA:
      DecodePush(&Cpu::MemPushA);
      break;
    case INST_PLP:
      DecodePull(&Cpu::MemPullP);
      break;
    case INST_PLA:
      DecodePull(&Cpu::MemPullA);
      break;
    case INST_SEC:
      DecodeNomem(&Cpu::DataSec);
      break;
    case INST_SEI:
      DecodeNomem(&Cpu::DataSei);
      break;
    case INST_SED:
      DecodeNomem(&Cpu::DataSed);
      break;
    case INST_CLI:
      DecodeNomem(&Cpu::DataCli);
      break;
    case INST_CLC:
      DecodeNomem(&Cpu::DataClc);
      break;
    case INST_CLD:
      DecodeNomem(&Cpu::DataCld);
      break;
    case INST_CLV:
      DecodeNomem(&Cpu::DataClv);
      break;
    case INST_DEY:
      DecodeNomem(&Cpu::DataDecY);
      break;
    case INST_DEX:
      DecodeNomem(&Cpu::DataDecX);
      break;
    case INST_INY:
      DecodeNomem(&Cpu::DataIncY);
      break;
    case INST_INX:
      DecodeNomem(&Cpu::DataIncX);
      break;
    case INST_TAY:
      DecodeNomem(&Cpu::DataMovAY);
      break;
    case INST_TYA:
      DecodeNomem(&Cpu::DataMovYA);
      break;
    case INST_TXA:
      DecodeNomem(&Cpu::DataMovXA);
      break;
    case INST_TXS:
      DecodeNomem(&Cpu::DataMovXS);
      break;
    case INST_TAX:
      DecodeNomem(&Cpu::DataMovAX);
      break;
    case INST_TSX:
      DecodeNomem(&Cpu::DataMovSX);
      break;
    case INST_NOP:
      DecodeNomem(&Cpu::Nop);
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
  state_->AddCycle(&Cpu::MemReadPcZpPtr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::DataAddPtrlX, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtr1Addrh, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page read instruction using the given micro operation.
 */
void Cpu::DecodeZp(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a read immediate instruction using the given micro operation.
 */
void Cpu::DecodeImm(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcMdr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeAbs(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an indirect indexed zero page read instruction using the given micro
 * operation.
 */
void Cpu::DecodeIzpY(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpPtr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtr1Addrh, &Cpu::DataAddAddrlY, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixaAddrh, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataAddAddrlX, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page Y indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpy(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataAddAddrlY, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAby(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::DataAddAddrlY, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixaAddrh, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAbx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::DataAddAddrlX, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixaAddrh, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues an implied (no memory access) read instruction using the given
 * micro operation.
 */
void Cpu::DecodeNomem(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, op, PC_INC);
  return;
}

/*
 * Queues a zero page read-write instruction using the given micro operation.
 */
void Cpu::DecodeRwZp(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwAbs(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read-write instruction using the given micro
 * operation.
 */
void Cpu::DecodeRwZpx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataAddAddrlX, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read-write instruction using the
 * given micro operation.
 */
void Cpu::DecodeRwAbx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::DataAddAddrlX, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixAddrh, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, op, PC_NOP);
  state_->AddCycle(&Cpu::MemWriteMdrAddr, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed indirect zero page write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWIzpx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpPtr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::DataAddPtrlX, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtr1Addrh, &Cpu::Nop, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues a zero page write instruction using the given micro operation.
 */
void Cpu::DecodeWZp(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWAbs(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::Nop, PC_INC);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an indirect addressed Y indexed zero page write instruction using the
 * given micro operation.
 */
void Cpu::DecodeWIzpY(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpPtr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPtrAddrl, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemReadPtr1Addrh, &Cpu::DataAddAddrlY, PC_NOP);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataAddAddrlX, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues a Y indexed zero page write instruction using the given micro
 * operation.
 */
void Cpu::DecodeWZpy(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcZpAddr, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataAddAddrlY, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAby(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::DataAddAddrlY, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed write instruction using the given
 * micro operation.
 */
void Cpu::DecodeWAbx(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcAddrl, &Cpu::Nop, PC_INC);
  state_->AddCycle(&Cpu::MemReadPcAddrh, &Cpu::DataAddAddrlX, PC_INC);
  state_->AddCycle(&Cpu::MemReadAddrMdr, &Cpu::DataFixAddrh, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues a stack push instruction using the given micro operation.
 */
void Cpu::DecodePush(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
  state_->AddCycle(op, &Cpu::DataDecS, PC_NOP);
  state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  return;
}

/*
 * Queues a stack pull instruction using the given micro operation.
 */
void Cpu::DecodePull(CpuOperation op) {
  state_->AddCycle(&Cpu::MemReadPcNodest, &Cpu::Nop, PC_NOP);
  state_->AddCycle(&Cpu::Nop, &Cpu::DataIncS, PC_NOP);
  state_->AddCycle(op, &Cpu::Nop, PC_NOP);
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
