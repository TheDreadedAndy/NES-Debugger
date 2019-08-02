/*
 * TODO: Update this.
 *
 * The microinstructions of the 6502 are handled by the state_t structure.
 * With the exception of brk, branch, and interrupt instructions, interrupt
 * polling happens before each fetch cycle. The plan is to handle these
 * special cases in cpu_run_cycle.
 *
 * More information on the CPU cycles can be found in nesdev.com//6502_cpu.txt
 * The microinstructions are an abstraction, the real cpu used a cycle counter
 * and an RLC to determine how the datapath should be controlled. It seemed
 * silly to reimplement that in code, as going that low level wouldn't be
 * helpful to accuracy.
 *
 * References to the APU, PPU, and IO are not found in this file, as they
 * are handled by MMIO and, thus, part of memory.c.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "./2A03.h"
#include "./regs.h"
#include "../memory/memory.h"
#include "../memory/header.h"
#include "./machinecode.h"
#include "./state.h"
#include "./micromem.h"
#include "./microdata.h"
#include "../ppu/ppu.h"

// DMA transfers take at least 513 cycles.
#define DMA_CYCLE_LENGTH 513

/* Global Variables */

// Global interrupt lines, accessable from outside this file.
bool irq_line = false;
bool nmi_line = false;

// Internal interrupt lines, used only in CPU emulation.
bool nmi_edge = false;
bool irq_level = false;

// Some instructions need to be able to poll on one cycle and interrupt later,
// this flag acknowledges these cases.
bool irq_ready = false;

// Used for DMA transfer to OAM. The transfer adds a cycle on odd frames.
bool cycle_even = false;
size_t dma_cycles_remaining = 0;
word_t dma_high = 0;

/* Helper functions. */
void cpu_run_mem(micro_t *micro);
void cpu_run_data(micro_t *micro);
bool cpu_can_poll(void);
void cpu_execute_dma(void);
void cpu_decode_inst(word_t inst);
void cpu_decode_izpx(microdata_t *micro_op);
void cpu_decode_zp(microdata_t *micro_op);
void cpu_decode_imm(microdata_t *micro_op);
void cpu_decode_abs(microdata_t *micro_op);
void cpu_decode_izp_y(microdata_t *micro_op);
void cpu_decode_zpx(microdata_t *micro_op);
void cpu_decode_zpy(microdata_t *micro_op);
void cpu_decode_abx(microdata_t *micro_op);
void cpu_decode_aby(microdata_t *micro_op);
void cpu_decode_nomem(microdata_t *micro_op);
void cpu_decode_rw_zp(microdata_t *micro_op);
void cpu_decode_rw_abs(microdata_t *micro_op);
void cpu_decode_rw_zpx(microdata_t *micro_op);
void cpu_decode_rw_abx(microdata_t *micro_op);
void cpu_decode_w_izpx(micromem_t *micro_op);
void cpu_decode_w_zp(micromem_t *micro_op);
void cpu_decode_w_abs(micromem_t *micro_op);
void cpu_decode_w_izp_y(micromem_t *micro_op);
void cpu_decode_w_zpx(micromem_t *micro_op);
void cpu_decode_w_zpy(micromem_t *micro_op);
void cpu_decode_w_abx(micromem_t *micro_op);
void cpu_decode_w_aby(micromem_t *micro_op);
void cpu_decode_push(micromem_t *micro_op);
void cpu_decode_pull(micromem_t *micro_op);
void cpu_poll_nmi_line(void);
void cpu_poll_irq_line(void);

/*
 * Initializes everything related to the cpu so that the emulation can begin.
 */
void cpu_init(FILE *rom_file, header_t *header) {
  // Init all cpu structures.
  memory_init(rom_file, header);
  state_init();
  regfile_init();

  // Load the reset location into the pc.
  R->pc_lo = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH);
  R->pc_hi = memory_read(MEMORY_RESET_LOW+1, MEMORY_RESET_HIGH);

  // Queue the first cycle to be emulated.
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * Executes the next cycle in the cpu emulation using the cpu structures.
 *
 * Assumes the cpu has been initialized.
 */
void cpu_run_cycle(void) {
  // Check if the CPU is suspended and executing a DMA.
  if (dma_cycles_remaining > 0) {
    cpu_execute_dma();
    cycle_even = !cycle_even;
    return;
  }

  // Poll the interrupt detectors, if it is time to do so.
  if (cpu_can_poll()) {
    // irq_ready should only be reset from a fetch call handling it
    // or from interrupts being blocked.
    irq_ready = (irq_ready || irq_level) && !((bool) (R->P & 0x04));
  }

  // Fetch and run the next micro instructions for the cycle.
  micro_t *next_micro = state_next_cycle();
  next_micro->mem();
  next_micro->data();
  if (next_micro->inc_pc) { regfile_inc_pc(); }

  // Poll the interrupt lines and update the detectors.
  cpu_poll_nmi_line();
  cpu_poll_irq_line();

  // Toggle the frame evenness.
  cycle_even = !cycle_even;

  return;
}

/*
 * Checks if the cpu should poll for interrupts on this cycle.
 *
 * Assumes that the provided structures are valid.
 */
bool cpu_can_poll(void) {
  // Interrupt polling (internal) happens when the cpu is about
  // to finish an instruction and said instruction is not an interrupt.
  return state_get_size() == 2 && R->inst != INST_BRK;
}

/*
 * Starts an OAM DMA transfer at the given address.
 *
 * Assumes that the CPU has been initialized.
 */
void cpu_start_dma(word_t addr) {
  // Starts the dma transfer by setting the high CPU memory byte and reseting
  // the cycle counter.
  dma_high = addr;
  dma_cycles_remaining = DMA_CYCLE_LENGTH;

  // If the CPU is on an odd cycle, the DMA takes one cycle longer.
  if (!cycle_even) { dma_cycles_remaining++; }

  return;
}

/*
 * Executes a cycle of the DMA transfer.
 *
 * Assumes the CPU has been initialized.
 */
void cpu_execute_dma(void) {
  // Current cpu memory address to read from (low byte, incremented every call
  // wraps back to zero at the end of every call).
  static word_t dma_low = 0;
  static word_t dma_mdr = 0;

  // The CPU is idle until there are <= 512 dma cycles remaining.
  if ((dma_cycles_remaining < DMA_CYCLE_LENGTH) && (dma_cycles_remaining & 1)) {
    // Odd cycle, so we write to OAM.
    ppu_oam_dma(dma_mdr);
  } else if (dma_cycles_remaining < DMA_CYCLE_LENGTH) {
    // Even cycle, so we read from memory.
    dma_mdr = memory_read(dma_low, dma_high);
  }
  dma_cycles_remaining--;
  dma_low++;

  return;
}

/*
 * Fetches the next instruction to be executing, storing brk instead
 * if an interrupt should be started. Adjusts the PC as necessary, since
 * branch calls which lead into a fetch do not.
 *
 * Assumes that all cpu structures have been initialized.
 * Should only be called from mem_fetch().
 */
void cpu_fetch(micro_t *micro) {
  // Fetch the next instruction to the instruction register.
  if (!nmi_edge && !irq_ready) {
    // A non-interrupt fetch should always be paired with a PC increment.
    // We set the micro ops pc_inc field here, in case we were coming
    // from a branch.
    micro->inc_pc = PC_INC;
    R->inst = memory_read(R->pc_lo, R->pc_hi);
  } else {
    // All interrupts fill the instruction register with 0x00 (BRK).
    R->inst = INST_BRK;
    // Interrupts should not increment the PC.
    micro->inc_pc = PC_NOP;
  }

  // Decode the instruction.
  cpu_decode_inst(R->inst);

  return;
}

/*
 * Decodes the next instruction (or interrupt) into micro instructions and
 * adds them to the state queue. Resets the interrupt flags if one is
 * processed.
 *
 * Assumes that all cpu structures have been initialized.
 * Should only be called from cpu_fetch().
 */
void cpu_decode_inst(word_t inst) {
  // We only decode the instruction if there are no interrupts.
  if (nmi_edge) {
    // An nmi signal has priority over an irq, and resets the ready flag for it.
    nmi_edge = false;
    irq_ready = false;

    // Since an nmi was detected, we queue its cycles and return.
    state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
    state_add_cycle(&mem_push_pch, &data_dec_s, PC_NOP);
    state_add_cycle(&mem_push_pcl, &data_dec_s, PC_NOP);
    state_add_cycle(&mem_push_p, &data_dec_s, PC_NOP);
    state_add_cycle(&mem_nmi_pcl, &data_sei, PC_NOP);
    state_add_cycle(&mem_nmi_pch, &data_nop, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
    return;
  } else if (irq_ready) {
    // The irq has been handled, so we reset the flag.
    irq_ready = false;

    // Since an irq was detected, we queue its cycles and return.
    state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
    state_add_cycle(&mem_push_pch, &data_dec_s, PC_NOP);
    state_add_cycle(&mem_push_pcl, &data_dec_s, PC_NOP);
    state_add_cycle(&mem_irq, &data_dec_s, PC_NOP);
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
  switch (inst) {
    case INST_ORA_IZPX:
      cpu_decode_izpx(&data_ora_mdr_a);
      break;
    case INST_ORA_ZP:
      cpu_decode_zp(&data_ora_mdr_a);
      break;
    case INST_ORA_IMM:
      cpu_decode_imm(&data_ora_mdr_a);
      break;
    case INST_ORA_ABS:
      cpu_decode_abs(&data_ora_mdr_a);
      break;
    case INST_ORA_IZP_Y:
      cpu_decode_izp_y(&data_ora_mdr_a);
      break;
    case INST_ORA_ZPX:
      cpu_decode_zpx(&data_ora_mdr_a);
      break;
    case INST_ORA_ABY:
      cpu_decode_aby(&data_ora_mdr_a);
      break;
    case INST_ORA_ABX:
      cpu_decode_abx(&data_ora_mdr_a);
      break;
    case INST_AND_IZPX:
      cpu_decode_izpx(&data_and_mdr_a);
      break;
    case INST_AND_ZP:
      cpu_decode_zp(&data_and_mdr_a);
      break;
    case INST_AND_IMM:
      cpu_decode_imm(&data_and_mdr_a);
      break;
    case INST_AND_ABS:
      cpu_decode_abs(&data_and_mdr_a);
      break;
    case INST_AND_IZP_Y:
      cpu_decode_izp_y(&data_and_mdr_a);
      break;
    case INST_AND_ZPX:
      cpu_decode_zpx(&data_and_mdr_a);
      break;
    case INST_AND_ABY:
      cpu_decode_aby(&data_and_mdr_a);
      break;
    case INST_AND_ABX:
      cpu_decode_abx(&data_and_mdr_a);
      break;
    case INST_EOR_IZPX:
      cpu_decode_izpx(&data_eor_mdr_a);
      break;
    case INST_EOR_ZP:
      cpu_decode_zp(&data_eor_mdr_a);
      break;
    case INST_EOR_IMM:
      cpu_decode_imm(&data_eor_mdr_a);
      break;
    case INST_EOR_ABS:
      cpu_decode_abs(&data_eor_mdr_a);
      break;
    case INST_EOR_IZP_Y:
      cpu_decode_izp_y(&data_eor_mdr_a);
      break;
    case INST_EOR_ZPX:
      cpu_decode_zpx(&data_eor_mdr_a);
      break;
    case INST_EOR_ABY:
      cpu_decode_aby(&data_eor_mdr_a);
      break;
    case INST_EOR_ABX:
      cpu_decode_abx(&data_eor_mdr_a);
      break;
    case INST_ADC_IZPX:
      cpu_decode_izpx(&data_adc_mdr_a);
      break;
    case INST_ADC_ZP:
      cpu_decode_zp(&data_adc_mdr_a);
      break;
    case INST_ADC_IMM:
      cpu_decode_imm(&data_adc_mdr_a);
      break;
    case INST_ADC_ABS:
      cpu_decode_abs(&data_adc_mdr_a);
      break;
    case INST_ADC_IZP_Y:
      cpu_decode_izp_y(&data_adc_mdr_a);
      break;
    case INST_ADC_ZPX:
      cpu_decode_zpx(&data_adc_mdr_a);
      break;
    case INST_ADC_ABY:
      cpu_decode_aby(&data_adc_mdr_a);
      break;
    case INST_ADC_ABX:
      cpu_decode_abx(&data_adc_mdr_a);
      break;
    case INST_STA_IZPX:
      cpu_decode_w_izpx(&mem_write_a_addr);
      break;
    case INST_STA_ZP:
      cpu_decode_w_zp(&mem_write_a_addr);
      break;
    case INST_STA_ABS:
      cpu_decode_w_abs(&mem_write_a_addr);
      break;
    case INST_STA_IZP_Y:
      cpu_decode_w_izp_y(&mem_write_a_addr);
      break;
    case INST_STA_ZPX:
      cpu_decode_w_zpx(&mem_write_a_addr);
      break;
    case INST_STA_ABY:
      cpu_decode_w_aby(&mem_write_a_addr);
      break;
    case INST_STA_ABX:
      cpu_decode_w_abx(&mem_write_a_addr);
      break;
    case INST_LDA_IZPX:
      cpu_decode_izpx(&data_mov_mdr_a);
      break;
    case INST_LDA_ZP:
      cpu_decode_zp(&data_mov_mdr_a);
      break;
    case INST_LDA_IMM:
      cpu_decode_imm(&data_mov_mdr_a);
      break;
    case INST_LDA_ABS:
      cpu_decode_abs(&data_mov_mdr_a);
      break;
    case INST_LDA_IZP_Y:
      cpu_decode_izp_y(&data_mov_mdr_a);
      break;
    case INST_LDA_ZPX:
      cpu_decode_zpx(&data_mov_mdr_a);
      break;
    case INST_LDA_ABY:
      cpu_decode_aby(&data_mov_mdr_a);
      break;
    case INST_LDA_ABX:
      cpu_decode_abx(&data_mov_mdr_a);
      break;
    case INST_CMP_IZPX:
      cpu_decode_izpx(&data_cmp_mdr_a);
      break;
    case INST_CMP_ZP:
      cpu_decode_zp(&data_cmp_mdr_a);
      break;
    case INST_CMP_IMM:
      cpu_decode_imm(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABS:
      cpu_decode_abs(&data_cmp_mdr_a);
      break;
    case INST_CMP_IZP_Y:
      cpu_decode_izp_y(&data_cmp_mdr_a);
      break;
    case INST_CMP_ZPX:
      cpu_decode_zpx(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABY:
      cpu_decode_aby(&data_cmp_mdr_a);
      break;
    case INST_CMP_ABX:
      cpu_decode_abx(&data_cmp_mdr_a);
      break;
    case INST_SBC_IZPX:
      cpu_decode_izpx(&data_sbc_mdr_a);
      break;
    case INST_SBC_ZP:
      cpu_decode_zp(&data_sbc_mdr_a);
      break;
    case INST_SBC_IMM:
      cpu_decode_imm(&data_sbc_mdr_a);
      break;
    case INST_SBC_ABS:
      cpu_decode_abs(&data_sbc_mdr_a);
      break;
    case INST_SBC_IZP_Y:
      cpu_decode_izp_y(&data_sbc_mdr_a);
      break;
    case INST_SBC_ZPX:
      cpu_decode_zpx(&data_sbc_mdr_a);
      break;
    case INST_SBC_ABY:
      cpu_decode_aby(&data_sbc_mdr_a);
      break;
    case INST_SBC_ABX:
      cpu_decode_abx(&data_sbc_mdr_a);
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
      cpu_decode_imm(&data_mov_mdr_x);
      break;
    case INST_LDX_ZP:
      cpu_decode_zp(&data_mov_mdr_x);
      break;
    case INST_LDX_ABS:
      cpu_decode_abs(&data_mov_mdr_x);
      break;
    case INST_LDX_ZPY:
      cpu_decode_zpy(&data_mov_mdr_x);
      break;
    case INST_LDX_ABY:
      cpu_decode_aby(&data_mov_mdr_x);
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
      cpu_decode_zp(&data_bit_mdr_a);
      break;
    case INST_BIT_ABS:
      cpu_decode_abs(&data_bit_mdr_a);
      break;
    case INST_JMP:
      state_add_cycle(&mem_read_pc_mdr, &data_nop, PC_INC);
      state_add_cycle(&mem_read_pc_pch, &data_mov_mdr_pcl, PC_NOP);
      state_add_cycle(&mem_fetch, &data_nop, PC_INC);
      break;
    case INST_JMPI:
      state_add_cycle(&mem_read_pc_ptrl, &data_nop, PC_INC);
      state_add_cycle(&mem_read_pc_ptrh, &data_nop, PC_INC);
      state_add_cycle(&mem_read_ptr_mdr, &data_nop, PC_NOP);
      state_add_cycle(&mem_read_ptr1_pch, &data_mov_mdr_pcl, PC_NOP);
      state_add_cycle(&mem_fetch, &data_nop, PC_INC);
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
      cpu_decode_imm(&data_mov_mdr_y);
      break;
    case INST_LDY_ZP:
      cpu_decode_zp(&data_mov_mdr_y);
      break;
    case INST_LDY_ABS:
      cpu_decode_abs(&data_mov_mdr_y);
      break;
    case INST_LDY_ZPX:
      cpu_decode_zpx(&data_mov_mdr_y);
      break;
    case INST_LDY_ABX:
      cpu_decode_abx(&data_mov_mdr_y);
      break;
    case INST_CPY_IMM:
      cpu_decode_imm(&data_cmp_mdr_y);
      break;
    case INST_CPY_ZP:
      cpu_decode_zp(&data_cmp_mdr_y);
      break;
    case INST_CPY_ABS:
      cpu_decode_abs(&data_cmp_mdr_y);
      break;
    case INST_CPX_IMM:
      cpu_decode_imm(&data_cmp_mdr_x);
      break;
    case INST_CPX_ZP:
      cpu_decode_zp(&data_cmp_mdr_x);
      break;
    case INST_CPX_ABS:
      cpu_decode_abs(&data_cmp_mdr_x);
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
      state_add_cycle(&mem_read_pc_mdr, &data_nop, PC_INC);
      state_add_cycle(&mem_nop, &data_branch, PC_NOP);
      break;
    case INST_BRK:
      state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_INC);
      state_add_cycle(&mem_push_pch, &data_dec_s, PC_NOP);
      state_add_cycle(&mem_push_pcl, &data_dec_s, PC_NOP);
      state_add_cycle(&mem_brk, &data_dec_s, PC_NOP);
      break;
    case INST_JSR:
      state_add_cycle(&mem_read_pc_mdr, &data_nop, PC_INC);
      state_add_cycle(&mem_nop, &data_nop, PC_NOP);
      state_add_cycle(&mem_push_pch, &data_dec_s, PC_NOP);
      state_add_cycle(&mem_push_pcl, &data_dec_s, PC_NOP);
      state_add_cycle(&mem_read_pc_pch, &data_mov_mdr_pcl, PC_NOP);
      state_add_cycle(&mem_fetch, &data_nop, PC_INC);
      break;
    case INST_RTI:
      state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
      state_add_cycle(&mem_nop, &data_inc_s, PC_NOP);
      state_add_cycle(&mem_pull_p, &data_inc_s, PC_NOP);
      state_add_cycle(&mem_pull_pcl, &data_inc_s, PC_NOP);
      state_add_cycle(&mem_pull_pch, &data_nop, PC_NOP);
      state_add_cycle(&mem_fetch, &data_nop, PC_INC);
      break;
    case INST_RTS:
      state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
      state_add_cycle(&mem_nop, &data_inc_s, PC_NOP);
      state_add_cycle(&mem_pull_pcl, &data_inc_s, PC_NOP);
      state_add_cycle(&mem_pull_pch, &data_nop, PC_NOP);
      state_add_cycle(&mem_nop, &data_nop, PC_INC);
      state_add_cycle(&mem_fetch, &data_nop, PC_INC);
      break;
    case INST_PHP:
      cpu_decode_push(&mem_push_p_b);
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
      cpu_decode_nomem(&data_sec);
      break;
    case INST_SEI:
      cpu_decode_nomem(&data_sei);
      break;
    case INST_SED:
      cpu_decode_nomem(&data_sed);
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
      cpu_decode_nomem(&data_nop);
      break;
    default:
      printf("Instruction %x is not implemented\n", inst);
      abort();
  }
  return;
}

/*
 * TODO
 */
void cpu_decode_izpx(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_ptr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_ptr_addrl, &data_add_ptrl_x, PC_NOP);
  state_add_cycle(&mem_read_ptr_addrl, &data_nop, PC_NOP);
  state_add_cycle(&mem_read_ptr1_addrh, &data_nop, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zp(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_imm(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_mdr, &data_nop, PC_INC);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_abs(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_izp_y(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_ptr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_ptr_addrl, &data_nop, PC_NOP);
  state_add_cycle(&mem_read_ptr1_addrh, &data_add_addrl_y, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_fixa_addrh, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpx(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_add_addrl_x, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpy(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_add_addrl_y, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_aby(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_add_addrl_y, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_fixa_addrh, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_abx(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_add_addrl_x, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_fixa_addrh, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_nomem(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zp(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abs(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zpx(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_add_addrl_x, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abx(microdata_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_add_addrl_x, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_fix_addrh, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_nop, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, micro_op, PC_NOP);
  state_add_cycle(&mem_write_mdr_addr, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izpx(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_ptr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_ptr_addrl, &data_add_ptrl_x, PC_NOP);
  state_add_cycle(&mem_read_ptr_addrl, &data_nop, PC_NOP);
  state_add_cycle(&mem_read_ptr1_addrh, &data_nop, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zp(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abs(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_nop, PC_INC);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izp_y(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_ptr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_ptr_addrl, &data_nop, PC_NOP);
  state_add_cycle(&mem_read_ptr1_addrh, &data_add_addrl_y, PC_NOP);
  state_add_cycle(&mem_read_addr_mdr, &data_fix_addrh, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpx(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_add_addrl_x, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpy(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_zp_addr, &data_nop, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_add_addrl_y, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_aby(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_add_addrl_y, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_fix_addrh, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abx(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_addrl, &data_nop, PC_INC);
  state_add_cycle(&mem_read_pc_addrh, &data_add_addrl_x, PC_INC);
  state_add_cycle(&mem_read_addr_mdr, &data_fix_addrh, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_push(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
  state_add_cycle(micro_op, &data_dec_s, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_pull(micromem_t *micro_op) {
  state_add_cycle(&mem_read_pc_nodest, &data_nop, PC_NOP);
  state_add_cycle(&mem_nop, &data_inc_s, PC_NOP);
  state_add_cycle(micro_op, &data_nop, PC_NOP);
  state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  return;
}

/*
 * Checks if the global nmi line has signaled for an interrupt using an
 * edge detector.
 *
 * When an nmi is detected, the edge signal stays high until it is handled.
 */
void cpu_poll_nmi_line(void) {
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
void cpu_poll_irq_line(void) {
  // The internal irq signal is a level detector that update every cycle.
  irq_level = irq_line;
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
