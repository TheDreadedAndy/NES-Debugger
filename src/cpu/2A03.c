/*
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
#include "./machinecode.h"
#include "./state.h"
#include "./microops.h"

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

/* Helper functions. */
void cpu_run_mem(micro_t *micro, regfile_t *R, memory_t *M, state_t *S);
void cpu_run_data(micro_t *micro, regfile_t *R, memory_t *M, state_t *S);
bool cpu_can_poll(regfile_t *R, state_t *S);
void cpu_fetch(micro_t *micro, regfile_t *R, memory_t *M, state_t *S);
void cpu_decode_inst(word_t inst, state_t *S);
void cpu_decode_izpx(microdata_t micro_op, state_t *S);
void cpu_decode_zp(microdata_t micro_op, state_t *S);
void cpu_decode_imm(microdata_t micro_op, state_t *S);
void cpu_decode_abs(microdata_t micro_op, state_t *S);
void cpu_decode_izp_y(microdata_t micro_op, state_t *S);
void cpu_decode_zpx(microdata_t micro_op, state_t *S);
void cpu_decode_zpy(microdata_t micro_op, state_t *S);
void cpu_decode_abx(microdata_t micro_op, state_t *S);
void cpu_decode_aby(microdata_t micro_op, state_t *S);
void cpu_decode_nomem(microdata_t micro_op, state_t *S);
void cpu_decode_rw_zp(microdata_t micro_op, state_t *S);
void cpu_decode_rw_abs(microdata_t micro_op, state_t *S);
void cpu_decode_rw_zpx(microdata_t micro_op, state_t *S);
void cpu_decode_rw_abx(microdata_t micro_op, state_t *S);
void cpu_decode_w_izpx(micromem_t micro_op, state_t *S);
void cpu_decode_w_zp(micromem_t micro_op, state_t *S);
void cpu_decode_w_abs(micromem_t micro_op, state_t *S);
void cpu_decode_w_izp_y(micromem_t micro_op, state_t *S);
void cpu_decode_w_zpx(micromem_t micro_op, state_t *S);
void cpu_decode_w_zpy(micromem_t micro_op, state_t *S);
void cpu_decode_w_abx(micromem_t micro_op, state_t *S);
void cpu_decode_w_aby(micromem_t micro_op, state_t *S);
void cpu_decode_push(micromem_t micro_op, state_t *S);
void cpu_decode_pull(micromem_t micro_op, state_t *S);
void cpu_poll_nmi_line(void);
void cpu_poll_irq_line(void);

/*
 * Takes in a register file, generic memory structure, and
 * cpu state structure. Uses these structures two execute the
 * next cycle in the cpu emulation.
 *
 * Assumes the provided structures are non-null and valid.
 */
void cpu_run_cycle(regfile_t *R, memory_t *M, state_t *S) {
  // Poll the interrupt detectors, if it is time to do so.
  if (cpu_can_poll(R, S)) {
    // irq_ready should only be reset from a fetch call handling it.
    // TODO: Should depend on the I flag.
    irq_ready |= irq_level;
  }

  // Fetch and run the next micro instructions for the cycle.
  micro_t *next_micro = state_next_cycle(S);
  cpu_run_mem(next_micro, R, M, S);
  cpu_run_data(next_micro, R, M, S);
  if (next_micro->inc_pc) { regfile_inc_pc(R); }

  // Poll the interrupt lines and update the detectors.
  cpu_poll_nmi_line();
  cpu_poll_irq_line();

  return;
}

/*
 * Checks if the cpu should poll for interrupts on this cycle.
 *
 * Assumes that the provided structures are valid.
 */
bool cpu_can_poll(regfile_t *R, state_t *S) {
  // Interrupt polling (internal) happens when the cpu is about
  // to finish an instruction and said instruction is not an interrupt.
  return state_get_size(S) == 2 && R->inst != INST_BRK;
}

/*
 * Executes the memory micro instruction contained in the field of the
 * micro opperation structure using the provided structures.
 *
 * Assumes the provided structures are valid.
 *
 * TODO: A new instruction should probably be added for interrupt hijacking.
 */
void cpu_run_mem(micro_t *micro, regfile_t *R, memory_t *M, state_t *S) {
  micromem_t mem = micro->mem;
  switch (mem) {
    case MEM_NOP:
      break;
    case MEM_FETCH:
      cpu_fetch(micro, R, M, S);
      break;
    case MEM_READ_PC_NODEST:
      memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_MDR:
      R->mdr = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_PCH:
      R->pc_hi = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_ZP_ADDR:
      R->addr_hi = 0;
      R->addr_lo = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_ADDRL:
      R->addr_lo = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_ADDRH:
      R->addr_hi = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_ZP_PTR:
      R->ptr_hi = 0;
      R->ptr_lo = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_PTRL:
      R->ptr_lo = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_PC_PTRH:
      R->ptr_hi = memory_read(R->pc_lo, R->pc_hi, M);
      break;
    case MEM_READ_ADDR_MDR:
      R->mdr = memory_read(R->addr_lo, R->addr_hi, M);
      break;
    case MEM_READ_PTR_MDR:
      R->mdr = memory_read(R->ptr_lo, R->ptr_hi, M);
      break;
    case MEM_READ_PTR_ADDRL:
      R->addr_lo = memory_read(R->ptr_lo, R->ptr_hi, M);
      break;
    case MEM_READ_PTR1_ADDRH:
      R->addr_hi = memory_read(R->ptr_lo + 1, R->ptr_hi, M);
      break;
    case MEM_READ_PTR1_PCH:
      R->pc_hi = memory_read(R->ptr_lo + 1, R->ptr_hi, M);
      break;
    case MEM_WRITE_MDR_ADDR:
      memory_write(R->mdr, R->addr_lo, R->addr_hi, M);
      break;
    case MEM_WRITE_A_ADDR:
      memory_write(R->A, R->addr_lo, R->addr_hi, M);
      break;
    case MEM_WRITE_X_ADDR:
      memory_write(R->X, R->addr_lo, R->addr_hi, M);
      break;
    case MEM_WRITE_Y_ADDR:
      memory_write(R->Y, R->addr_lo, R->addr_hi, M);
      break;
    case MEM_PUSH_PCL:
      memory_write(R->pc_lo, R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PUSH_PCH:
      memory_write(R->pc_hi, R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PUSH_A:
      memory_write(R->A, R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PUSH_P:
      memory_write((R->P | 0x20), R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PUSH_P_B:
      // Push P and set bit 4.
      memory_write((R->P | 0x30), R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_PCL:
      R->pc_lo = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_PCH:
      R->pc_hi = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_A:
      R->A = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_P:
      // Pulling P ignores bits 4 and 5.
      R->P = memory_read(R->S, MEMORY_STACK_HIGH, M) & 0xCF;
      break;
    case MEM_NMI_PCL:
      R->pc_lo = memory_read(MEMORY_NMI_LOW, MEMORY_NMI_HIGH, M);
      break;
    case MEM_NMI_PCH:
      R->pc_hi = memory_read(MEMORY_NMI_LOW + 1, MEMORY_NMI_HIGH, M);
      break;
    case MEM_RESET_PCL:
      R->pc_lo = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH, M);
      break;
    case MEM_RESET_PCH:
      R->pc_hi = memory_read(MEMORY_RESET_LOW + 1, MEMORY_RESET_HIGH, M);
      break;
    case MEM_IRQ_PCL:
      R->pc_lo = memory_read(MEMORY_IRQ_LOW, MEMORY_IRQ_HIGH, M);
      break;
    case MEM_IRQ_PCH:
      R->pc_hi = memory_read(MEMORY_IRQ_LOW + 1, MEMORY_IRQ_HIGH, M);
      break;
  }
  return;
}

/*
 * Executes the data opperation of the given micro instruction using
 * the provided structures.
 *
 * Assumes the provided structures are valid.
 */
void cpu_run_data(micro_t *micro, regfile_t *R, memory_t *M, state_t *S) {
  // Some data operations require temperary storage.
  dword_t res;
  word_t carry, ovf, flag;
  bool cond, taken;

  /*
   * TODO
   */
  switch (micro->data) {
    case DAT_NOP:
      break;
    case DAT_INC_S:
      // Flags are not set for S, since it's part of push/pull.
      R->S++;
      break;
    case DAT_INC_X:
      R->X++;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_INC_Y:
      R->Y++;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    case DAT_INC_MDR:
      // Memory Data Register is used to visual modification on memory.
      R->mdr++;
      R->P = (R->P & 0x7D) | (R->mdr & 0x80) | ((R->mdr == 0) << 1);
      break;
    case DAT_DEC_S:
      R->S--;
      break;
    case DAT_DEC_X:
      R->X--;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_DEC_Y:
      R->Y--;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    case DAT_DEC_MDR:
      R->mdr--;
      R->P = (R->P & 0x7D) | (R->mdr & 0x80) | ((R->mdr == 0) << 1);
      break;
    case DAT_MOV_A_X:
      R->X = R->A;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_MOV_A_Y:
      R->Y = R->A;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    case DAT_MOV_S_X:
      R->X = R->S;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_MOV_X_A:
      R->A = R->X;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_MOV_X_S:
      // Flags are not set when changing S.
      R->S = R->X;
      break;
    case DAT_MOV_Y_A:
      R->A = R->Y;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_MOV_MDR_PCL:
      R->pc_lo = R->mdr;
      break;
    case DAT_MOV_MDR_A:
      R->A = R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_MOV_MDR_X:
      R->X = R->mdr;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_MOV_MDR_Y:
      R->Y = R->mdr;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    case DAT_CLC:
      R->P = R->P & 0xFE;
      break;
    case DAT_CLD:
      R->P = R->P & 0xF7;
      break;
    case DAT_CLI:
      R->P = R->P & 0xFB;
      break;
    case DAT_CLV:
      R->P = R->P & 0xBF;
      break;
    case DAT_SEC:
      R->P = R->P | 0x01;
      break;
    case DAT_SED:
      R->P = R->P | 0x08;
      break;
    case DAT_SEI:
      R->P = R->P | 0x04;
      break;
    case DAT_CMP_MDR_A:
      // Carry flag is unsigned overflow.
      res = (dword_t)R->A + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_CMP_MDR_X:
      res = (dword_t)R->X + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_CMP_MDR_Y:
      res = (dword_t)R->Y + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_ASL_MDR:
      // The shifted out bit is stored in the carry flag for shifting ops.
      carry = R->mdr >> 7;
      R->mdr = R->mdr << 1;
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    case DAT_ASL_A:
      carry = R->A >> 7;
      R->A = R->A << 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_LSR_MDR:
      carry = R->mdr & 0x01;
      R->mdr = R->mdr >> 1;
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    case DAT_LSR_A:
      carry = R->A & 0x01;
      R->A = R->A >> 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_ROL_MDR:
      carry = R->mdr >> 7;
      R->mdr = (R->mdr << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    case DAT_ROL_A:
      carry = R->A >> 7;
      R->A = (R->A << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_ROR_MDR:
      carry = R->mdr & 0x01;
      R->mdr = (R->mdr >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    case DAT_ROR_A:
      carry = R->A & 0x01;
      R->A = (R->A >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_EOR_MDR_A:
      R->A = R->A ^ R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_AND_MDR_A:
      R->A = R->A & R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_ORA_MDR_A:
      R->A = R->A | R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_ADC_MDR_A:
      res = (dword_t)R->A + (dword_t)R->mdr + (dword_t)(R->P & 0x01);
      ovf = ((R->A & 0x80) == (R->mdr & 0x80)) && ((R->A & 0x80) != (res & 0x80));
      R->A = (word_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
           | (ovf << 6) | (res >> 8);
      break;
    case DAT_SBC_MDR_A:
      res = (dword_t)R->A + (dword_t)(-R->mdr) + (dword_t)(-(R->P & 0x01));
      ovf = ((R->A & 0x80) == (R->mdr & 0x80)) && ((R->A & 0x80) != (res & 0x80));
      R->A = (word_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
           | (ovf << 6) | (res >> 8);
      break;
    case DAT_BIT_MDR_A:
      R->P = (R->P & 0x3D) | (R->mdr & 0xC0) | (((R->A & R->mdr) == 0) << 1);
      break;
    case DAT_ADD_ADDRL_X:
      res = (dword_t)R->addr_lo + (dword_t)R->X;
      R->addr_lo = (word_t)res;
      R->carry = res >> 8;
      break;
    case DAT_ADD_ADDRL_Y:
      res = (dword_t)R->addr_lo + (dword_t)R->X;
      R->addr_lo = (word_t)res;
      R->carry = res >> 8;
      break;
    case DAT_ADD_PTRL_X:
      R->ptr_lo = R->ptr_lo + R->X;
      break;
    case DAT_FIXA_ADDRH:
      if (R->carry) {
        R->addr_hi += R->carry;
        state_push_cycle(micro->mem, DAT_NOP, false, S);
      }
      R->carry = 0;
      break;
    case DAT_FIX_ADDRH:
      R->addr_hi = R->addr_hi + R->carry;
      break;
    case DAT_FIX_PCH:
      R->pc_hi = R->pc_hi + R->carry;
      break;
    /*
     * Branch instructions are of the form xxy10000, and are broken into
     * three cases:
     * 1) If the flag indicated by xx has value y, then the reletive address
     * is added to the PC.
     * 2) If case 1 results in a page crossing on the pc, an extra cycle is
     * added.
     * 3) If xx does not have value y, this micro op is the same as MEM_FETCH.
     */
    case DAT_BRANCH:
      // Calculate whether or not the branch was taken.
      flag = R->inst >> 6;
      cond = (bool)((R->inst >> 5) & 1);
      // Black magic that pulls the proper flag from the status reg.
      flag = (flag >> 1) ? ((R->P >> (flag - 2)) & 1)
                         : ((R->P >> (7 - flag)) & 1);
      taken = (((bool)flag) == cond);

      // Add the reletive address to pc_lo. Reletive addressing is signed,
      // so we need to sign extend the mdr before we add it to pc_lo.
      res = (dword_t)R->pc_lo + (dword_t)(int16_t)(int8_t)R->mdr;
      R->carry = (word_t)(res >> 8);

      // Execute the proper cycles according to the above results.
      if (!taken) {
        // Case 3.
        cpu_fetch(micro, R, M, S);
      } else if (R->carry) {
        // Case 2.
        R->pc_lo = (word_t)res;
        state_add_cycle(MEM_NOP, DAT_FIX_PCH, false, S);
        state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      } else {
        // Case 1.
        R->pc_lo = (word_t)res;
        state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      }

      break;
  }
  return;
}

/*
 * TODO
 */
void cpu_fetch(micro_t *micro, regfile_t *R, memory_t *M, state_t *S) {
  // Fetch the next instruction to the instruction register.
  if (!nmi_edge && !irq_ready) {
    // A non-interrupt fetch should always be paired with a PC increment.
    // We set the micro ops pc_inc field here, in case we were coming
    // from a branch.
    micro->inc_pc = PC_INC;
    R->inst = memory_read(R->pc_lo, R->pc_hi, M);
  } else {
    // All interrupts fill the instruction register with 0x00 (BRK).
    R->inst = INST_BRK;
    // Interrupts should not increment the PC.
    micro->inc_pc = PC_NOP;
  }

  // Decode the instruction.
  cpu_decode_inst(R->inst, S);

  return;
}

// Decode the instruction and queues the necessary micro instructions.
void cpu_decode_inst(word_t inst, state_t *S) {
  // We only decode the instruction if there are no interrupts.
  if (nmi_edge) {
    // An nmi signal has priority over an irq, and resets the ready flag for it.
    nmi_edge = false;
    irq_ready = false;
    //TODO: Add micro ops.
    return;
  } else if (irq_ready) {
    // The irq has been handled, so we reset the flag.
    irq_ready = false;
    //TODO: Add micro ops.
    return;
  }

  /*
   * TODO: Expand this explanation.
   * No interrupt, so we procede as normal and add the proper micro ops.
   */
  switch (inst) {
    case INST_ORA_IZPX:
      cpu_decode_izpx(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_ZP:
      cpu_decode_zp(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_IMM:
      cpu_decode_imm(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_ABS:
      cpu_decode_abs(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_IZP_Y:
      cpu_decode_izp_y(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_ZPX:
      cpu_decode_zpx(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_ABY:
      cpu_decode_aby(DAT_ORA_MDR_A, S);
      break;
    case INST_ORA_ABX:
      cpu_decode_abx(DAT_ORA_MDR_A, S);
      break;
    case INST_AND_IZPX:
      cpu_decode_izpx(DAT_AND_MDR_A, S);
      break;
    case INST_AND_ZP:
      cpu_decode_zp(DAT_AND_MDR_A, S);
      break;
    case INST_AND_IMM:
      cpu_decode_imm(DAT_AND_MDR_A, S);
      break;
    case INST_AND_ABS:
      cpu_decode_abs(DAT_AND_MDR_A, S);
      break;
    case INST_AND_IZP_Y:
      cpu_decode_izp_y(DAT_AND_MDR_A, S);
      break;
    case INST_AND_ZPX:
      cpu_decode_zpx(DAT_AND_MDR_A, S);
      break;
    case INST_AND_ABY:
      cpu_decode_aby(DAT_AND_MDR_A, S);
      break;
    case INST_AND_ABX:
      cpu_decode_abx(DAT_AND_MDR_A, S);
      break;
    case INST_EOR_IZPX:
      cpu_decode_izpx(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_ZP:
      cpu_decode_zp(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_IMM:
      cpu_decode_imm(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_ABS:
      cpu_decode_abs(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_IZP_Y:
      cpu_decode_izp_y(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_ZPX:
      cpu_decode_zpx(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_ABY:
      cpu_decode_aby(DAT_EOR_MDR_A, S);
      break;
    case INST_EOR_ABX:
      cpu_decode_abx(DAT_EOR_MDR_A, S);
      break;
    case INST_ADC_IZPX:
      cpu_decode_izpx(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_ZP:
      cpu_decode_zp(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_IMM:
      cpu_decode_imm(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_ABS:
      cpu_decode_abs(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_IZP_Y:
      cpu_decode_izp_y(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_ZPX:
      cpu_decode_zpx(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_ABY:
      cpu_decode_aby(DAT_ADC_MDR_A, S);
      break;
    case INST_ADC_ABX:
      cpu_decode_abx(DAT_ADC_MDR_A, S);
      break;
    case INST_STA_IZPX:
      cpu_decode_w_izpx(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_ZP:
      cpu_decode_w_zp(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_ABS:
      cpu_decode_w_abs(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_IZP_Y:
      cpu_decode_w_izp_y(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_ZPX:
      cpu_decode_w_zpx(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_ABY:
      cpu_decode_w_aby(MEM_WRITE_A_ADDR, S);
      break;
    case INST_STA_ABX:
      cpu_decode_w_abx(MEM_WRITE_A_ADDR, S);
      break;
    case INST_LDA_IZPX:
      cpu_decode_izpx(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_ZP:
      cpu_decode_zp(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_IMM:
      cpu_decode_imm(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_ABS:
      cpu_decode_abs(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_IZP_Y:
      cpu_decode_izp_y(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_ZPX:
      cpu_decode_zpx(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_ABY:
      cpu_decode_aby(DAT_MOV_MDR_A, S);
      break;
    case INST_LDA_ABX:
      cpu_decode_abx(DAT_MOV_MDR_A, S);
      break;
    case INST_CMP_IZPX:
      cpu_decode_izpx(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_ZP:
      cpu_decode_zp(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_IMM:
      cpu_decode_imm(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_ABS:
      cpu_decode_abs(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_IZP_Y:
      cpu_decode_izp_y(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_ZPX:
      cpu_decode_zpx(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_ABY:
      cpu_decode_aby(DAT_CMP_MDR_A, S);
      break;
    case INST_CMP_ABX:
      cpu_decode_abx(DAT_CMP_MDR_A, S);
      break;
    case INST_SBC_IZPX:
      cpu_decode_izpx(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_ZP:
      cpu_decode_zp(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_IMM:
      cpu_decode_imm(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_ABS:
      cpu_decode_abs(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_IZP_Y:
      cpu_decode_izp_y(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_ZPX:
      cpu_decode_zpx(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_ABY:
      cpu_decode_aby(DAT_SBC_MDR_A, S);
      break;
    case INST_SBC_ABX:
      cpu_decode_abx(DAT_SBC_MDR_A, S);
      break;
    case INST_ASL_ZP:
      cpu_decode_rw_zp(DAT_ASL_MDR, S);
      break;
    case INST_ASL_ACC:
      cpu_decode_nomem(DAT_ASL_A, S);
      break;
    case INST_ASL_ABS:
      cpu_decode_rw_abs(DAT_ASL_MDR, S);
      break;
    case INST_ASL_ZPX:
      cpu_decode_rw_zpx(DAT_ASL_MDR, S);
      break;
    case INST_ASL_ABX:
      cpu_decode_rw_abx(DAT_ASL_MDR, S);
      break;
    case INST_ROL_ZP:
      cpu_decode_rw_zp(DAT_ROL_MDR, S);
      break;
    case INST_ROL_ACC:
      cpu_decode_nomem(DAT_ROL_A, S);
      break;
    case INST_ROL_ABS:
      cpu_decode_rw_abs(DAT_ROL_MDR, S);
      break;
    case INST_ROL_ZPX:
      cpu_decode_rw_zpx(DAT_ROL_MDR, S);
      break;
    case INST_ROL_ABX:
      cpu_decode_rw_abx(DAT_ROL_MDR, S);
      break;
    case INST_LSR_ZP:
      cpu_decode_rw_zp(DAT_LSR_MDR, S);
      break;
    case INST_LSR_ACC:
      cpu_decode_nomem(DAT_LSR_A, S);
      break;
    case INST_LSR_ABS:
      cpu_decode_rw_abs(DAT_LSR_MDR, S);
      break;
    case INST_LSR_ZPX:
      cpu_decode_rw_zpx(DAT_LSR_MDR, S);
      break;
    case INST_LSR_ABX:
      cpu_decode_rw_abx(DAT_LSR_MDR, S);
      break;
    case INST_ROR_ZP:
      cpu_decode_rw_zp(DAT_ROR_MDR, S);
      break;
    case INST_ROR_ACC:
      cpu_decode_nomem(DAT_ROR_A, S);
      break;
    case INST_ROR_ABS:
      cpu_decode_rw_abs(DAT_ROR_MDR, S);
      break;
    case INST_ROR_ZPX:
      cpu_decode_rw_zpx(DAT_ROR_MDR, S);
      break;
    case INST_ROR_ABX:
      cpu_decode_rw_abx(DAT_ROR_MDR, S);
      break;
    case INST_STX_ZP:
      cpu_decode_w_zp(MEM_WRITE_X_ADDR, S);
      break;
    case INST_STX_ABS:
      cpu_decode_w_abs(MEM_WRITE_X_ADDR, S);
      break;
    case INST_STX_ZPY:
      cpu_decode_w_zpy(MEM_WRITE_X_ADDR, S);
      break;
    case INST_LDX_IMM:
      cpu_decode_imm(DAT_MOV_MDR_X, S);
      break;
    case INST_LDX_ZP:
      cpu_decode_zp(DAT_MOV_MDR_X, S);
      break;
    case INST_LDX_ABS:
      cpu_decode_abs(DAT_MOV_MDR_X, S);
      break;
    case INST_LDX_ZPY:
      cpu_decode_zpy(DAT_MOV_MDR_X, S);
      break;
    case INST_LDX_ABY:
      cpu_decode_aby(DAT_MOV_MDR_X, S);
      break;
    case INST_DEC_ZP:
      cpu_decode_rw_zp(DAT_DEC_MDR, S);
      break;
    case INST_DEC_ABS:
      cpu_decode_rw_abs(DAT_DEC_MDR, S);
      break;
    case INST_DEC_ZPX:
      cpu_decode_rw_zpx(DAT_DEC_MDR, S);
      break;
    case INST_DEC_ABX:
      cpu_decode_rw_abx(DAT_DEC_MDR, S);
      break;
    case INST_INC_ZP:
      cpu_decode_rw_zp(DAT_INC_MDR, S);
      break;
    case INST_INC_ABS:
      cpu_decode_rw_abs(DAT_INC_MDR, S);
      break;
    case INST_INC_ZPX:
      cpu_decode_rw_zpx(DAT_INC_MDR, S);
      break;
    case INST_INC_ABX:
      cpu_decode_rw_abx(DAT_INC_MDR, S);
      break;
    case INST_BIT_ZP:
      cpu_decode_zp(DAT_BIT_MDR_A, S);
      break;
    case INST_BIT_ABS:
      cpu_decode_abs(DAT_BIT_MDR_A, S);
      break;
    case INST_JMP:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_PCH, DAT_MOV_MDR_PCL, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_JMPI:
      state_add_cycle(MEM_READ_PC_PTRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_PTRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PTR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_READ_PTR1_PCH, DAT_MOV_MDR_PCL, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STY_ZP:
      cpu_decode_w_zp(MEM_WRITE_Y_ADDR, S);
      break;
    case INST_STY_ABS:
      cpu_decode_w_abs(MEM_WRITE_Y_ADDR, S);
      break;
    case INST_STY_ZPX:
      cpu_decode_w_zpx(MEM_WRITE_Y_ADDR, S);
      break;
    case INST_LDY_IMM:
      cpu_decode_imm(DAT_MOV_MDR_Y, S);
      break;
    case INST_LDY_ZP:
      cpu_decode_zp(DAT_MOV_MDR_Y, S);
      break;
    case INST_LDY_ABS:
      cpu_decode_abs(DAT_MOV_MDR_Y, S);
      break;
    case INST_LDY_ZPX:
      cpu_decode_zpx(DAT_MOV_MDR_Y, S);
      break;
    case INST_LDY_ABX:
      cpu_decode_abx(DAT_MOV_MDR_Y, S);
      break;
    case INST_CPY_IMM:
      cpu_decode_imm(DAT_CMP_MDR_Y, S);
      break;
    case INST_CPY_ZP:
      cpu_decode_zp(DAT_CMP_MDR_Y, S);
      break;
    case INST_CPY_ABS:
      cpu_decode_abs(DAT_CMP_MDR_Y, S);
      break;
    case INST_CPX_IMM:
      cpu_decode_imm(DAT_CMP_MDR_X, S);
      break;
    case INST_CPX_ZP:
      cpu_decode_zp(DAT_CMP_MDR_X, S);
      break;
    case INST_CPX_ABS:
      cpu_decode_abs(DAT_CMP_MDR_X, S);
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
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_NOP, DAT_BRANCH, false, S);
      break;
    case INST_BRK:
      //TODO: Fix interrupt hijacking. The I flag needs to be set.
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, true, S);
      state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, false, S);
      state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, false, S);
      state_add_cycle(MEM_PUSH_P_B, DAT_DEC_S, false, S);
      state_add_cycle(MEM_IRQ_PCL, DAT_NOP, false, S);
      state_add_cycle(MEM_IRQ_PCH, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_JSR:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_NOP, DAT_NOP, false, S);
      state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, false, S);
      state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, false, S);
      state_add_cycle(MEM_READ_PC_PCH, DAT_MOV_MDR_PCL, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_RTI:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_NOP, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_P, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_PCL, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_PCH, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_RTS:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_NOP, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_PCL, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_PCH, DAT_NOP, false, S);
      state_add_cycle(MEM_NOP, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_PHP:
      cpu_decode_push(MEM_PUSH_P_B, S);
      break;
    case INST_PHA:
      cpu_decode_push(MEM_PUSH_A, S);
      break;
    case INST_PLP:
      cpu_decode_pull(MEM_PULL_P, S);
      break;
    case INST_PLA:
      cpu_decode_pull(MEM_PULL_A, S);
      break;
    case INST_SEC:
      cpu_decode_nomem(DAT_SEC, S);
      break;
    case INST_SEI:
      cpu_decode_nomem(DAT_SEI, S);
      break;
    case INST_SED:
      cpu_decode_nomem(DAT_SED, S);
      break;
    case INST_CLI:
      cpu_decode_nomem(DAT_CLI, S);
      break;
    case INST_CLC:
      cpu_decode_nomem(DAT_CLC, S);
      break;
    case INST_CLD:
      cpu_decode_nomem(DAT_CLD, S);
      break;
    case INST_CLV:
      cpu_decode_nomem(DAT_CLV, S);
      break;
    case INST_DEY:
      cpu_decode_nomem(DAT_DEC_Y, S);
      break;
    case INST_DEX:
      cpu_decode_nomem(DAT_DEC_X, S);
      break;
    case INST_INY:
      cpu_decode_nomem(DAT_INC_Y, S);
      break;
    case INST_INX:
      cpu_decode_nomem(DAT_INC_X, S);
      break;
    case INST_TAY:
      cpu_decode_nomem(DAT_MOV_A_Y, S);
      break;
    case INST_TYA:
      cpu_decode_nomem(DAT_MOV_Y_A, S);
      break;
    case INST_TXA:
      cpu_decode_nomem(DAT_MOV_X_A, S);
      break;
    case INST_TXS:
      cpu_decode_nomem(DAT_MOV_X_S, S);
      break;
    case INST_TAX:
      cpu_decode_nomem(DAT_MOV_A_X, S);
      break;
    case INST_TSX:
      cpu_decode_nomem(DAT_MOV_S_X, S);
      break;
    case INST_NOP:
      cpu_decode_nomem(DAT_NOP, S);
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
void cpu_decode_izpx(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_ADD_PTRL_X, PC_NOP, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_zp(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_imm(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_abs(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_izp_y(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_ADD_ADDRL_Y, PC_NOP, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpx(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, PC_NOP, S);
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpy(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_Y, PC_NOP, S);
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_aby(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_Y, PC_INC, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_abx(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, PC_INC, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_nomem(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zp(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, PC_NOP, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, PC_NOP, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abs(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zpx(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abx(microdata_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false, S);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izpx(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_ADD_PTRL_X, false, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, false, S);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_NOP, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zp(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abs(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izp_y(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, false, S);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_ADD_ADDRL_Y, false, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpx(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpy(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_Y, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_aby(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_Y, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abx(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, true, S);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_push(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
  state_add_cycle(micro_op, DAT_DEC_S, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_decode_pull(micromem_t micro_op, state_t *S) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
  state_add_cycle(MEM_NOP, DAT_INC_S, false, S);
  state_add_cycle(micro_op, DAT_NOP, false, S);
  state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
  return;
}

/*
 * TODO
 */
void cpu_poll_nmi_line(void) {
  // The internal nmi signal is edge sensitive, so we should only set it
  // when the previous value was false and the current value is true.
  static bool nmi_prev = false;
  if (nmi_line == true && nmi_prev == false) {
    nmi_prev = nmi_line;
    nmi_edge = true;
  } else {
    // The internal nmi signal should only be reset from a fetch call
    // handling it, so we do nothing in this case.
    nmi_prev = nmi_line;
  }
  return;
}

/*
 * TODO
 */
void cpu_poll_irq_line(void) {
  // The internal irq signal is a level detector that update every cycle.
  irq_level = irq_line;
  return;
}
