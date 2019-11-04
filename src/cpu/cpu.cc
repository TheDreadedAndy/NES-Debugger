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

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../memory/memory.h"
#include "../memory/header.h"
#include "./machinecode.h"
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
static void cpu_decode_inst(void) {
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
      cpu_decode_w_izpx(&MemWriteAAddr);
      break;
    case INST_STA_ZP:
      cpu_decode_w_zp(&MemWriteAAddr);
      break;
    case INST_STA_ABS:
      cpu_decode_w_abs(&MemWriteAAddr);
      break;
    case INST_STA_IZP_Y:
      cpu_decode_w_izp_y(&MemWriteAAddr);
      break;
    case INST_STA_ZPX:
      cpu_decode_w_zpx(&MemWriteAAddr);
      break;
    case INST_STA_ABY:
      cpu_decode_w_aby(&MemWriteAAddr);
      break;
    case INST_STA_ABX:
      cpu_decode_w_abx(&MemWriteAAddr);
      break;
    case INST_LDA_IZPX:
      DecodeIzpx(&data_mov_mdr_a);
      break;
    case INST_LDA_ZP:
      DecodeZp(&data_mov_mdr_a);
      break;
    case INST_LDA_IMM:
      DecodeImm(&data_mov_mdr_a);
      break;
    case INST_LDA_ABS:
      DecodeAbs(&data_mov_mdr_a);
      break;
    case INST_LDA_IZP_Y:
      DecodeIzpY(&data_mov_mdr_a);
      break;
    case INST_LDA_ZPX:
      DecodeZpx(&data_mov_mdr_a);
      break;
    case INST_LDA_ABY:
      DecodeAby(&data_mov_mdr_a);
      break;
    case INST_LDA_ABX:
      DecodeAbx(&data_mov_mdr_a);
      break;
    case INST_CMP_IZPX:
      DecodeIzpx(&data_cmp_mdr_a);
      break;
    case INST_CMP_ZP:
      DecodeZp(&data_cmp_mdr_a);
      break;
    case INST_CMP_IMM:
      DecodeImm(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABS:
      DecodeAbs(&data_cmp_mdr_a);
      break;
    case INST_CMP_IZP_Y:
      DecodeIzpY(&data_cmp_mdr_a);
      break;
    case INST_CMP_ZPX:
      DecodeZpx(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABY:
      DecodeAby(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABX:
      DecodeAbx(&data_cmp_mdr_a);
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
      cpu_decode_rw_zp(&data_asl_mdr);
      break;
    case INST_ASL_ACC:
      cpu_decode_nomem(&data_asl_a);
      break;
    case INST_ASL_ABS:
      cpu_decode_rw_abs(&data_asl_mdr);
      break;
    case INST_ASL_ZPX:
      cpu_decode_rw_zpx(&data_asl_mdr);
      break;
    case INST_ASL_ABX:
      cpu_decode_rw_abx(&data_asl_mdr);
      break;
    case INST_ROL_ZP:
      cpu_decode_rw_zp(&data_rol_mdr);
      break;
    case INST_ROL_ACC:
      cpu_decode_nomem(&data_rol_a);
      break;
    case INST_ROL_ABS:
      cpu_decode_rw_abs(&data_rol_mdr);
      break;
    case INST_ROL_ZPX:
      cpu_decode_rw_zpx(&data_rol_mdr);
      break;
    case INST_ROL_ABX:
      cpu_decode_rw_abx(&data_rol_mdr);
      break;
    case INST_LSR_ZP:
      cpu_decode_rw_zp(&data_lsr_mdr);
      break;
    case INST_LSR_ACC:
      cpu_decode_nomem(&data_lsr_a);
      break;
    case INST_LSR_ABS:
      cpu_decode_rw_abs(&data_lsr_mdr);
      break;
    case INST_LSR_ZPX:
      cpu_decode_rw_zpx(&data_lsr_mdr);
      break;
    case INST_LSR_ABX:
      cpu_decode_rw_abx(&data_lsr_mdr);
      break;
    case INST_ROR_ZP:
      cpu_decode_rw_zp(&data_ror_mdr);
      break;
    case INST_ROR_ACC:
      cpu_decode_nomem(&data_ror_a);
      break;
    case INST_ROR_ABS:
      cpu_decode_rw_abs(&data_ror_mdr);
      break;
    case INST_ROR_ZPX:
      cpu_decode_rw_zpx(&data_ror_mdr);
      break;
    case INST_ROR_ABX:
      cpu_decode_rw_abx(&data_ror_mdr);
      break;
    case INST_STX_ZP:
      cpu_decode_w_zp(&mem_write_x_addr);
      break;
    case INST_STX_ABS:
      cpu_decode_w_abs(&mem_write_x_addr);
      break;
    case INST_STX_ZPY:
      cpu_decode_w_zpy(&mem_write_x_addr);
      break;
    case INST_LDX_IMM:
      DecodeImm(&data_mov_mdr_x);
      break;
    case INST_LDX_ZP:
      DecodeZp(&data_mov_mdr_x);
      break;
    case INST_LDX_ABS:
      DecodeAbs(&data_mov_mdr_x);
      break;
    case INST_LDX_ZPY:
      DecodeZpy(&data_mov_mdr_x);
      break;
    case INST_LDX_ABY:
      DecodeAby(&data_mov_mdr_x);
      break;
    case INST_DEC_ZP:
      cpu_decode_rw_zp(&data_dec_mdr);
      break;
    case INST_DEC_ABS:
      cpu_decode_rw_abs(&data_dec_mdr);
      break;
    case INST_DEC_ZPX:
      cpu_decode_rw_zpx(&data_dec_mdr);
      break;
    case INST_DEC_ABX:
      cpu_decode_rw_abx(&data_dec_mdr);
      break;
    case INST_INC_ZP:
      cpu_decode_rw_zp(&data_inc_mdr);
      break;
    case INST_INC_ABS:
      cpu_decode_rw_abs(&data_inc_mdr);
      break;
    case INST_INC_ZPX:
      cpu_decode_rw_zpx(&data_inc_mdr);
      break;
    case INST_INC_ABX:
      cpu_decode_rw_abx(&data_inc_mdr);
      break;
    case INST_BIT_ZP:
      DecodeZp(&data_bit_mdr_a);
      break;
    case INST_BIT_ABS:
      DecodeAbs(&data_bit_mdr_a);
      break;
    case INST_JMP:
      state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
      state_->AddCycle(&mem_read_pc_pch, &data_mov_mdr_pcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_JMPI:
      state_->AddCycle(&mem_read_pc_ptrl, &Nop, PC_INC);
      state_->AddCycle(&mem_read_pc_ptrh, &Nop, PC_INC);
      state_->AddCycle(&mem_read_ptr_mdr, &Nop, PC_NOP);
      state_->AddCycle(&mem_read_ptr1_pch, &data_mov_mdr_pcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_STY_ZP:
      cpu_decode_w_zp(&mem_write_y_addr);
      break;
    case INST_STY_ABS:
      cpu_decode_w_abs(&mem_write_y_addr);
      break;
    case INST_STY_ZPX:
      cpu_decode_w_zpx(&mem_write_y_addr);
      break;
    case INST_LDY_IMM:
      DecodeImm(&data_mov_mdr_y);
      break;
    case INST_LDY_ZP:
      DecodeZp(&data_mov_mdr_y);
      break;
    case INST_LDY_ABS:
      DecodeAbs(&data_mov_mdr_y);
      break;
    case INST_LDY_ZPX:
      DecodeZpx(&data_mov_mdr_y);
      break;
    case INST_LDY_ABX:
      DecodeAbx(&data_mov_mdr_y);
      break;
    case INST_CPY_IMM:
      DecodeImm(&data_cmp_mdr_y);
      break;
    case INST_CPY_ZP:
      DecodeZp(&data_cmp_mdr_y);
      break;
    case INST_CPY_ABS:
      DecodeAbs(&data_cmp_mdr_y);
      break;
    case INST_CPX_IMM:
      DecodeImm(&data_cmp_mdr_x);
      break;
    case INST_CPX_ZP:
      DecodeZp(&data_cmp_mdr_x);
      break;
    case INST_CPX_ABS:
      DecodeAbs(&data_cmp_mdr_x);
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
      state_->AddCycle(&Nop, &data_branch, PC_NOP);
      break;
    case INST_BRK:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_INC);
      state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
      state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
      state_->AddCycle(&mem_brk, &DataDecS, PC_NOP);
      break;
    case INST_JSR:
      state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
      state_->AddCycle(&Nop, &Nop, PC_NOP);
      state_->AddCycle(&MemPushPch, &DataDecS, PC_NOP);
      state_->AddCycle(&MemPushPcl, &DataDecS, PC_NOP);
      state_->AddCycle(&mem_read_pc_pch, &data_mov_mdr_pcl, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_RTI:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &DataIncS, PC_NOP);
      state_->AddCycle(&mem_pull_p, &DataIncS, PC_NOP);
      state_->AddCycle(&mem_pull_pcl, &DataIncS, PC_NOP);
      state_->AddCycle(&mem_pull_pch, &Nop, PC_NOP);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_RTS:
      state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &DataIncS, PC_NOP);
      state_->AddCycle(&mem_pull_pcl, &DataIncS, PC_NOP);
      state_->AddCycle(&mem_pull_pch, &Nop, PC_NOP);
      state_->AddCycle(&Nop, &Nop, PC_INC);
      state_->AddCycle(&MemFetch, &Nop, PC_INC);
      break;
    case INST_PHP:
      cpu_decode_push(&MemPushPB);
      break;
    case INST_PHA:
      cpu_decode_push(&mem_push_a);
      break;
    case INST_PLP:
      cpu_decode_pull(&mem_pull_p);
      break;
    case INST_PLA:
      cpu_decode_pull(&mem_pull_a);
      break;
    case INST_SEC:
      cpu_decode_nomem(&DataSec);
      break;
    case INST_SEI:
      cpu_decode_nomem(&DataSei);
      break;
    case INST_SED:
      cpu_decode_nomem(&DataSed);
      break;
    case INST_CLI:
      cpu_decode_nomem(&data_cli);
      break;
    case INST_CLC:
      cpu_decode_nomem(&data_clc);
      break;
    case INST_CLD:
      cpu_decode_nomem(&data_cld);
      break;
    case INST_CLV:
      cpu_decode_nomem(&data_clv);
      break;
    case INST_DEY:
      cpu_decode_nomem(&data_dec_y);
      break;
    case INST_DEX:
      cpu_decode_nomem(&data_dec_x);
      break;
    case INST_INY:
      cpu_decode_nomem(&data_inc_y);
      break;
    case INST_INX:
      cpu_decode_nomem(&data_inc_x);
      break;
    case INST_TAY:
      cpu_decode_nomem(&data_mov_a_y);
      break;
    case INST_TYA:
      cpu_decode_nomem(&data_mov_y_a);
      break;
    case INST_TXA:
      cpu_decode_nomem(&data_mov_x_a);
      break;
    case INST_TXS:
      cpu_decode_nomem(&data_mov_x_s);
      break;
    case INST_TAX:
      cpu_decode_nomem(&data_mov_a_x);
      break;
    case INST_TSX:
      cpu_decode_nomem(&data_mov_s_x);
      break;
    case INST_NOP:
      cpu_decode_nomem(&Nop);
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
void Cpu::DecodeIzpx(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_ptr, &Nop, PC_INC);
  state_->AddCycle(&mem_read_ptr_addrl, &data_add_ptrl_x, PC_NOP);
  state_->AddCycle(&mem_read_ptr_addrl, &Nop, PC_NOP);
  state_->AddCycle(&mem_read_ptr1_addrh, &Nop, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues a zero page read instruction using the given micro operation.
 */
void Cpu::DecodeZp(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues a read immediate instruction using the given micro operation.
 */
void Cpu::DecodeImm(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcMdr, &Nop, PC_INC);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeAbs(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues an indirect indexed zero page read instruction using the given micro
 * operation.
 */
void Cpu::DecodeIzpY(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_ptr, &Nop, PC_INC);
  state_->AddCycle(&mem_read_ptr_addrl, &Nop, PC_NOP);
  state_->AddCycle(&mem_read_ptr1_addrh, &data_add_addrl_y, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &data_fixa_addrh, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpx(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues a zero page Y indexed read instruction using the given micro
 * operation.
 */
void Cpu::DecodeZpy(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &data_add_addrl_y, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAby(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &data_add_addrl_y, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &data_fixa_addrh, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read instruction using the given
 * micro operation.
 */
void Cpu::DecodeAbx(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &data_fixa_addrh, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues an implied (no memory access) read instruction using the given
 * micro operation.
 */
static void cpu_decode_nomem(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, micro_op, PC_INC);
  return;
}

/*
 * Queues a zero page read-write instruction using the given micro operation.
 */
static void cpu_decode_rw_zp(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed read-write instruction using the given micro
 * operation.
 */
static void cpu_decode_rw_abs(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read-write instruction using the given micro
 * operation.
 */
static void cpu_decode_rw_zpx(microdata_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read-write instruction using the
 * given micro operation.
 */
static void cpu_decode_rw_abx(microdata_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &Nop, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_->AddCycle(&mem_write_mdr_addr, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed indirect zero page write instruction using the given
 * micro operation.
 */
static void cpu_decode_w_izpx(micromem_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_ptr, &Nop, PC_INC);
  state_->AddCycle(&mem_read_ptr_addrl, &data_add_ptrl_x, PC_NOP);
  state_->AddCycle(&mem_read_ptr_addrl, &Nop, PC_NOP);
  state_->AddCycle(&mem_read_ptr1_addrh, &Nop, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a zero page write instruction using the given micro operation.
 */
static void cpu_decode_w_zp(micromem_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed write instruction using the given micro
 * operation.
 */
static void cpu_decode_w_abs(micromem_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &Nop, PC_INC);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an indirect addressed Y indexed zero page write instruction using the
 * given micro operation.
 */
static void cpu_decode_w_izp_y(micromem_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_ptr, &Nop, PC_INC);
  state_->AddCycle(&mem_read_ptr_addrl, &Nop, PC_NOP);
  state_->AddCycle(&mem_read_ptr1_addrh, &data_add_addrl_y, PC_NOP);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an X indexed zero page write instruction using the given micro
 * operation.
 */
static void cpu_decode_w_zpx(micromem_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataAddAddrlX, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a Y indexed zero page write instruction using the given micro
 * operation.
 */
static void cpu_decode_w_zpy(micromem_t *micro_op) {
  state_->AddCycle(&mem_read_pc_zp_addr, &Nop, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &data_add_addrl_y, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed Y indexed write instruction using the given
 * micro operation.
 */
static void cpu_decode_w_aby(micromem_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &data_add_addrl_y, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed write instruction using the given
 * micro operation.
 */
static void cpu_decode_w_abx(micromem_t *micro_op) {
  state_->AddCycle(&MemReadPcAddrl, &Nop, PC_INC);
  state_->AddCycle(&MemReadPcAddrh, &DataAddAddrlX, PC_INC);
  state_->AddCycle(&MemReadAddrMdr, &DataFixAddrh, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a stack push instruction using the given micro operation.
 */
static void cpu_decode_push(micromem_t *micro_op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(micro_op, &DataDecS, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Queues a stack pull instruction using the given micro operation.
 */
static void cpu_decode_pull(micromem_t *micro_op) {
  state_->AddCycle(&MemReadPcNodest, &Nop, PC_NOP);
  state_->AddCycle(&Nop, &DataIncS, PC_NOP);
  state_->AddCycle(micro_op, &Nop, PC_NOP);
  state_->AddCycle(&MemFetch, &Nop, PC_INC);
  return;
}

/*
 * Checks if the global nmi line has signaled for an interrupt using an
 * edge detector.
 *
 * When an nmi is detected, the edge signal stays high until it is handled.
 */
static void cpu_poll_nmi_line(void) {
  // The internal nmi signal is rising edge sensitive, so we should only set it
  // when the previous value was false and the current value is true.
  static bool nmi_prev = false;
  if (nmi_line && !nmi_prev) { nmi_edge = true; }
  nmi_prev = nmi_line;
  return;
}

/*
 * Checks if the global irq line has signaled for an interrupt.
 */
static void cpu_poll_irq_line(void) {
  // The internal irq signal is a level detector that update every cycle.
  irq_level = (irq_line > 0);
  return;
}

/*
 * Frees the register file, memory, and state.
 *
 * Assumes all three have been initialized
 */
void cpu_free(void) {
  free(R);
  memory_free();
  state_free();
}
