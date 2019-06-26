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
#include "../memory/header.h"
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

// The register file is used extremely often, so the global regfile is aliased
// to this shortcut during the initialization of the cpu.
regfile_t *R = NULL;

/* Helper functions. */
void cpu_run_mem(micro_t *micro);
void cpu_run_data(micro_t *micro);
bool cpu_can_poll();
void cpu_fetch(micro_t *micro);
void cpu_decode_inst(word_t inst);
void cpu_decode_izpx(microdata_t micro_op);
void cpu_decode_zp(microdata_t micro_op);
void cpu_decode_imm(microdata_t micro_op);
void cpu_decode_abs(microdata_t micro_op);
void cpu_decode_izp_y(microdata_t micro_op);
void cpu_decode_zpx(microdata_t micro_op);
void cpu_decode_zpy(microdata_t micro_op);
void cpu_decode_abx(microdata_t micro_op);
void cpu_decode_aby(microdata_t micro_op);
void cpu_decode_nomem(microdata_t micro_op);
void cpu_decode_rw_zp(microdata_t micro_op);
void cpu_decode_rw_abs(microdata_t micro_op);
void cpu_decode_rw_zpx(microdata_t micro_op);
void cpu_decode_rw_abx(microdata_t micro_op);
void cpu_decode_w_izpx(micromem_t micro_op);
void cpu_decode_w_zp(micromem_t micro_op);
void cpu_decode_w_abs(micromem_t micro_op);
void cpu_decode_w_izp_y(micromem_t micro_op);
void cpu_decode_w_zpx(micromem_t micro_op);
void cpu_decode_w_zpy(micromem_t micro_op);
void cpu_decode_w_abx(micromem_t micro_op);
void cpu_decode_w_aby(micromem_t micro_op);
void cpu_decode_push(micromem_t micro_op);
void cpu_decode_pull(micromem_t micro_op);
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
  system_regfile->pc_lo = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH);
  system_regfile->pc_hi = memory_read(MEMORY_RESET_LOW+1, MEMORY_RESET_HIGH);

  // Alias the global regfile to the shortcut.
  R = system_regfile;

  // Queue the first cycle to be emulated.
  state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
  return;
}

/*
 * Takes in a register file, generic memory structure, and
 * cpu state structure. Uses these structures two execute the
 * next cycle in the cpu emulation.
 *
 * Assumes the provided structures are non-null and valid.
 */
void cpu_run_cycle() {
  // Poll the interrupt detectors, if it is time to do so.
  if (cpu_can_poll()) {
    // irq_ready should only be reset from a fetch call handling it
    // or from interrupts being blocked.
    irq_ready = (irq_ready || irq_level) && !((bool) (R->P & 0x04));
  }

  // Fetch and run the next micro instructions for the cycle.
  micro_t *next_micro = state_next_cycle();
  cpu_run_mem(next_micro);
  cpu_run_data(next_micro);
  if (next_micro->inc_pc) { regfile_inc_pc(); }

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
bool cpu_can_poll() {
  // Interrupt polling (internal) happens when the cpu is about
  // to finish an instruction and said instruction is not an interrupt.
  return state_get_size() == 2 && R->inst != INST_BRK;
}

/*
 * Executes the memory micro instruction contained in the field of the
 * micro opperation structure using the provided structures.
 *
 * Assumes the provided structures are valid.
 */
void cpu_run_mem(micro_t *micro) {
  micromem_t mem = micro->mem;
  switch (mem) {
    case MEM_NOP:
      break;
    case MEM_FETCH:
      cpu_fetch(micro);
      break;
    case MEM_READ_PC_NODEST:
      memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_MDR:
      R->mdr = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_PCH:
      R->pc_hi = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_ZP_ADDR:
      R->addr_hi = 0;
      R->addr_lo = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_ADDRL:
      R->addr_lo = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_ADDRH:
      R->addr_hi = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_ZP_PTR:
      R->ptr_hi = 0;
      R->ptr_lo = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_PTRL:
      R->ptr_lo = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_PC_PTRH:
      R->ptr_hi = memory_read(R->pc_lo, R->pc_hi);
      break;
    case MEM_READ_ADDR_MDR:
      R->mdr = memory_read(R->addr_lo, R->addr_hi);
      break;
    case MEM_READ_PTR_MDR:
      R->mdr = memory_read(R->ptr_lo, R->ptr_hi);
      break;
    case MEM_READ_PTR_ADDRL:
      R->addr_lo = memory_read(R->ptr_lo, R->ptr_hi);
      break;
    case MEM_READ_PTR1_ADDRH:
      R->addr_hi = memory_read(R->ptr_lo + 1, R->ptr_hi);
      break;
    case MEM_READ_PTR1_PCH:
      R->pc_hi = memory_read(R->ptr_lo + 1, R->ptr_hi);
      break;
    case MEM_WRITE_MDR_ADDR:
      memory_write(R->mdr, R->addr_lo, R->addr_hi);
      break;
    case MEM_WRITE_A_ADDR:
      memory_write(R->A, R->addr_lo, R->addr_hi);
      break;
    case MEM_WRITE_X_ADDR:
      memory_write(R->X, R->addr_lo, R->addr_hi);
      break;
    case MEM_WRITE_Y_ADDR:
      memory_write(R->Y, R->addr_lo, R->addr_hi);
      break;
    case MEM_PUSH_PCL:
      memory_write(R->pc_lo, R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PUSH_PCH:
      memory_write(R->pc_hi, R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PUSH_A:
      memory_write(R->A, R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PUSH_P:
      // Push P and clear the B flag.
      memory_write((R->P | 0x20), R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PUSH_P_B:
      // Push P and set the B flag.
      memory_write((R->P | 0x30), R->S, MEMORY_STACK_HIGH);
      break;
    // Pushes the state register on the stack with the B flag set, then adds the
    // next cycles of the interrupt according to hijacking behavior.
    case MEM_BRK:
      memory_write((R->P | 0x30), R->S, MEMORY_STACK_HIGH);

      // Allows an nmi to hijack the brk instruction.
      if (nmi_edge) {
        state_add_cycle(MEM_NMI_PCL, DAT_SEI, PC_NOP);
        state_add_cycle(MEM_NMI_PCH, DAT_NOP, PC_NOP);
        state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
      } else {
        state_add_cycle(MEM_IRQ_PCL, DAT_SEI, PC_NOP);
        state_add_cycle(MEM_IRQ_PCH, DAT_NOP, PC_NOP);
        state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
      }

      break;
    // Pushes the state register on the stack with the B flag clear, then adds
    // the nexxt cycles of the interrupt according to hijacking behavior.
    case MEM_IRQ:
      memory_write((R->P | 0x20), R->S, MEMORY_STACK_HIGH);

      // Allows an nmi to hijack an irq interrupt.
      if (nmi_edge) {
        state_add_cycle(MEM_NMI_PCL, DAT_SEI, PC_NOP);
        state_add_cycle(MEM_NMI_PCH, DAT_NOP, PC_NOP);
        state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
      } else {
        state_add_cycle(MEM_IRQ_PCL, DAT_SEI, PC_NOP);
        state_add_cycle(MEM_IRQ_PCH, DAT_NOP, PC_NOP);
        state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
      }

      break;
    case MEM_PULL_PCL:
      R->pc_lo = memory_read(R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PULL_PCH:
      R->pc_hi = memory_read(R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PULL_A:
      R->A = memory_read(R->S, MEMORY_STACK_HIGH);
      break;
    case MEM_PULL_P:
      // Pulling P ignores bits 4 and 5.
      R->P = memory_read(R->S, MEMORY_STACK_HIGH) & 0xCF;
      break;
    case MEM_NMI_PCL:
      R->pc_lo = memory_read(MEMORY_NMI_LOW, MEMORY_NMI_HIGH);
      break;
    case MEM_NMI_PCH:
      R->pc_hi = memory_read(MEMORY_NMI_LOW + 1, MEMORY_NMI_HIGH);
      break;
    case MEM_RESET_PCL:
      R->pc_lo = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH);
      break;
    case MEM_RESET_PCH:
      R->pc_hi = memory_read(MEMORY_RESET_LOW + 1, MEMORY_RESET_HIGH);
      break;
    case MEM_IRQ_PCL:
      R->pc_lo = memory_read(MEMORY_IRQ_LOW, MEMORY_IRQ_HIGH);
      break;
    case MEM_IRQ_PCH:
      R->pc_hi = memory_read(MEMORY_IRQ_LOW + 1, MEMORY_IRQ_HIGH);
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
void cpu_run_data(micro_t *micro) {
  // Some data operations require temperary storage.
  dword_t res;
  word_t carry, ovf, flag;
  bool cond, taken;

  /*
   * Whenever a data oppertion is performed, there is a good chance that the
   * cpu status will need to be updated. The cpu status is represented by
   * a register with 7 flags, which are layed out in the following way:
   * Bit:  7 6 5 4 3 2 1 0
   * flag: N V B B D I Z C
   *       | | | | | | | |
   *       | | | | | | | -> Carry out for unsigned arithmetic.
   *       | | | | | | ---> Zero flag, set when result was zero.
   *       | | | | | -----> IRQ Block flag, prevents IRQ's from being triggered.
   *       | | | | -------> BCD Flag, useless in the NES.
   *       | | -----------> The "B" flag, used to determine where an interrupt
   *       | |              originated.
   *       | -------------> Signed overflow flag.
   *       ---------------> Negative flag, equal to the MSB of the result.
   *
   * The hex values in the code that follows are used to mask in or out these
   * flags.
   */
  switch (micro->data) {
    // I mean, yeah.
    case DAT_NOP:
      break;
    // Increments the S register. Used in push/pull. Does not set flags.
    case DAT_INC_S:
      R->S++;
      break;
    // Increments the X register. Sets the N and Z flags.
    case DAT_INC_X:
      R->X++;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    // Increments the Y register. Sets the N and Z flags.
    case DAT_INC_Y:
      R->Y++;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    // Increments the MDR. Sets the N and Z flags.
    case DAT_INC_MDR:
      R->mdr++;
      R->P = (R->P & 0x7D) | (R->mdr & 0x80) | ((R->mdr == 0) << 1);
      break;
    // Decrements the S register. Used in push/pull. Does not set flags.
    case DAT_DEC_S:
      R->S--;
      break;
    // Decrements the X register. Sets the N and Z flags.
    case DAT_DEC_X:
      R->X--;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    // Decrements the Y register. Sets the N and Z flags.
    case DAT_DEC_Y:
      R->Y--;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    // Decrements the MDR. Sets the N and Z flags.
    case DAT_DEC_MDR:
      R->mdr--;
      R->P = (R->P & 0x7D) | (R->mdr & 0x80) | ((R->mdr == 0) << 1);
      break;
    // Copies the value stored in A to X. Sets the N and Z flags.
    case DAT_MOV_A_X:
      R->X = R->A;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    // Copies the value stored in A to Y. Sets the N and Z flags.
    case DAT_MOV_A_Y:
      R->Y = R->A;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    // Copies the value stored in S to X. Sets the N and Z flags.
    case DAT_MOV_S_X:
      R->X = R->S;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    // Copies the value stored in X to A. Sets the N and Z flags.
    case DAT_MOV_X_A:
      R->A = R->X;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // Copies the value stored in X to S. Sets no flag (S is the stack pointer).
    case DAT_MOV_X_S:
      R->S = R->X;
      break;
    // Copies the value stored in Y to A. Sets the N and Z flags.
    case DAT_MOV_Y_A:
      R->A = R->Y;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // Copies the value stored in the MDR to the PCL register. Sets no flags.
    case DAT_MOV_MDR_PCL:
      R->pc_lo = R->mdr;
      break;
    // Copies the value stored in the MDR to Register A. Sets the N and Z flags.
    case DAT_MOV_MDR_A:
      R->A = R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // Copies the value stored in the MDR to Register X. Sets the N and Z flags.
    case DAT_MOV_MDR_X:
      R->X = R->mdr;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    // Copies the value stored in the MDR to Register X. Sets the N and Z flags.
    case DAT_MOV_MDR_Y:
      R->Y = R->mdr;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    // Clears the carry flag.
    case DAT_CLC:
      R->P = R->P & 0xFE;
      break;
    // Clears the decimal flag.
    case DAT_CLD:
      R->P = R->P & 0xF7;
      break;
    // Clears the interrupt flag.
    case DAT_CLI:
      R->P = R->P & 0xFB;
      break;
    // Clears the overflow flag.
    case DAT_CLV:
      R->P = R->P & 0xBF;
      break;
    // Sets the carry flag.
    case DAT_SEC:
      R->P = R->P | 0x01;
      break;
    // Sets the decimal flag.
    case DAT_SED:
      R->P = R->P | 0x08;
      break;
    // Sets the interrupt flag.
    case DAT_SEI:
      R->P = R->P | 0x04;
      break;
    // Subtracts the MDR from Register A, and stores the N, Z, and C flags
    // of the result.
    case DAT_CMP_MDR_A:
      // The carry flag is unsigned overflow. We use a double word to hold
      // the extra bit.
      res = (dword_t)R->A + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    // Subtracts the MDR from Register X, and stores the N, Z, and C flags
    // of the result.
    case DAT_CMP_MDR_X:
      res = (dword_t)R->X + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    // Subtracts the MDR from Register Y, and stores the N, Z, and C flags
    // of the result.
    case DAT_CMP_MDR_Y:
      res = (dword_t)R->Y + (dword_t)(-R->mdr);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    // Shifts the MDR left once, storing the lost bit in C and setting the
    // N and Z flags.
    case DAT_ASL_MDR:
      carry = R->mdr >> 7;
      R->mdr = R->mdr << 1;
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    // Shifts A left once, storing the lost bit in C and setting the
    // N and Z flags.
    case DAT_ASL_A:
      carry = R->A >> 7;
      R->A = R->A << 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    // Shifts the MDR right once, storing the lost bit in C and setting the
    // N and Z flags.
    case DAT_LSR_MDR:
      carry = R->mdr & 0x01;
      R->mdr = R->mdr >> 1;
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    // Shifts A right once, storing the lost bit in C and setting the
    // N and Z flags.
    case DAT_LSR_A:
      carry = R->A & 0x01;
      R->A = R->A >> 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    // Shifts the MDR left once, back filling with C. Stores the lost bit in C,
    // sets the N and Z flags.
    case DAT_ROL_MDR:
      carry = R->mdr >> 7;
      R->mdr = (R->mdr << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    // Shifts A left once, back filling with C. Stores teh lost bit in C, sets
    // the N and Z flags.
    case DAT_ROL_A:
      carry = R->A >> 7;
      R->A = (R->A << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    // Shifts the MDR right once, back filling with C. Stores the lost bit in C,
    // sets the N and Z flags.
    case DAT_ROR_MDR:
      carry = R->mdr & 0x01;
      R->mdr = (R->mdr >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->mdr & 0x80) | ((R->mdr == 0) << 1) | carry;
      break;
    // Shifts A right once, back filling with C. Stores the lost bit in C,
    // sets the N and Z flags.
    case DAT_ROR_A:
      carry = R->A & 0x01;
      R->A = (R->A >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    // XOR's the MDR and A. Sets the N and Z flags.
    case DAT_EOR_MDR_A:
      R->A = R->A ^ R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // AND's the MDR and A. Sets the N and Z flags.
    case DAT_AND_MDR_A:
      R->A = R->A & R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // OR's the MDR and A. Sets the N and Z flags.
    case DAT_ORA_MDR_A:
      R->A = R->A | R->mdr;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    // Adds the MDR, A, and the C flag, storing the result in A.
    // Sets the N, V, Z, and C flags.
    case DAT_ADC_MDR_A:
      res = (dword_t)R->A + (dword_t)R->mdr + (dword_t)(R->P & 0x01);
      ovf = ((R->A & 0x80) == (R->mdr & 0x80))
         && ((R->A & 0x80) != (res & 0x80));
      R->A = (word_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
                           | (ovf << 6) | (res >> 8);
      break;
    // Subtracts the MDR from A, using C as a borrow flag. The result
    // is equal to A - MDR - (1 - C). Sets the N, V, Z, and C flags.
    case DAT_SBC_MDR_A:
      // See documentation for proof of this line. Gives the correct ressult
      // without issues in the carry out.
      res = (dword_t)R->A + (dword_t)(~R->mdr) + (dword_t)(R->P & 0x01);
      ovf = ((R->A & 0x80) == (R->mdr & 0x80))
         && ((R->A & 0x80) != (res & 0x80));
      R->A = (word_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
           | (ovf << 6) | (res >> 8);
      break;
    // Moves the two high bits of A to the state register (N and V).
    // Sets the zero flag according to A AND MDR.
    case DAT_BIT_MDR_A:
      R->P = (R->P & 0x3D) | (R->mdr & 0xC0) | (((R->A & R->mdr) == 0) << 1);
      break;
    // Adds X to the low address byte, storing the carry out in the carry
    // abstraction register.
    case DAT_ADD_ADDRL_X:
      res = (dword_t)R->addr_lo + (dword_t)R->X;
      R->addr_lo = (word_t)res;
      R->carry = res >> 8;
      break;
    // Adds Y to the low address byte, storing the carry out in the carry
    // abstraction register.
    case DAT_ADD_ADDRL_Y:
      res = (dword_t)R->addr_lo + (dword_t)R->X;
      R->addr_lo = (word_t)res;
      R->carry = res >> 8;
      break;
    // Adds X to the low pointer byte. Page crossings are ignored.
    case DAT_ADD_PTRL_X:
      R->ptr_lo = R->ptr_lo + R->X;
      break;
    // Performs the last addressing operation again if the address crossed a
    // page bound and needed to be fixed.
    case DAT_FIXA_ADDRH:
      if (R->carry) {
        R->addr_hi += R->carry;
        state_push_cycle(micro->mem, DAT_NOP, false);
      }
      break;
    // Adds the carry out from the last addressing data operation to addr_hi.
    case DAT_FIX_ADDRH:
      R->addr_hi = R->addr_hi + R->carry;
      break;
    // Adds the carry out from the last addressing data operation to PCH.
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
        cpu_fetch(micro);
      } else if (R->carry) {
        // Case 2.
        R->pc_lo = (word_t)res;
        state_add_cycle(MEM_NOP, DAT_FIX_PCH, false);
        state_add_cycle(MEM_FETCH, DAT_NOP, true);
      } else {
        // Case 1.
        R->pc_lo = (word_t)res;
        state_add_cycle(MEM_FETCH, DAT_NOP, true);
      }

      break;
  }
  return;
}

/*
 * TODO
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

// Decode the instruction and queues the necessary micro instructions.
void cpu_decode_inst(word_t inst) {
  // We only decode the instruction if there are no interrupts.
  if (nmi_edge) {
    // An nmi signal has priority over an irq, and resets the ready flag for it.
    nmi_edge = false;
    irq_ready = false;

    // Since an nmi was detected, we queue its cycles and return.
    state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, PC_NOP);
    state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, PC_NOP);
    state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, PC_NOP);
    state_add_cycle(MEM_PUSH_P, DAT_DEC_S, PC_NOP);
    state_add_cycle(MEM_NMI_PCL, DAT_SEI, PC_NOP);
    state_add_cycle(MEM_NMI_PCH, DAT_NOP, PC_NOP);
    state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
    return;
  } else if (irq_ready) {
    // The irq has been handled, so we reset the flag.
    irq_ready = false;

    // Since an irq was detected, we queue its cycles and return.
    state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, PC_NOP);
    state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, PC_NOP);
    state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, PC_NOP);
    state_add_cycle(MEM_IRQ, DAT_DEC_S, PC_NOP);
    return;
  }

  /*
   * TODO: Expand this explanation.
   * No interrupt, so we procede as normal and add the proper micro ops.
   */
  switch (inst) {
    case INST_ORA_IZPX:
      cpu_decode_izpx(DAT_ORA_MDR_A);
      break;
    case INST_ORA_ZP:
      cpu_decode_zp(DAT_ORA_MDR_A);
      break;
    case INST_ORA_IMM:
      cpu_decode_imm(DAT_ORA_MDR_A);
      break;
    case INST_ORA_ABS:
      cpu_decode_abs(DAT_ORA_MDR_A);
      break;
    case INST_ORA_IZP_Y:
      cpu_decode_izp_y(DAT_ORA_MDR_A);
      break;
    case INST_ORA_ZPX:
      cpu_decode_zpx(DAT_ORA_MDR_A);
      break;
    case INST_ORA_ABY:
      cpu_decode_aby(DAT_ORA_MDR_A);
      break;
    case INST_ORA_ABX:
      cpu_decode_abx(DAT_ORA_MDR_A);
      break;
    case INST_AND_IZPX:
      cpu_decode_izpx(DAT_AND_MDR_A);
      break;
    case INST_AND_ZP:
      cpu_decode_zp(DAT_AND_MDR_A);
      break;
    case INST_AND_IMM:
      cpu_decode_imm(DAT_AND_MDR_A);
      break;
    case INST_AND_ABS:
      cpu_decode_abs(DAT_AND_MDR_A);
      break;
    case INST_AND_IZP_Y:
      cpu_decode_izp_y(DAT_AND_MDR_A);
      break;
    case INST_AND_ZPX:
      cpu_decode_zpx(DAT_AND_MDR_A);
      break;
    case INST_AND_ABY:
      cpu_decode_aby(DAT_AND_MDR_A);
      break;
    case INST_AND_ABX:
      cpu_decode_abx(DAT_AND_MDR_A);
      break;
    case INST_EOR_IZPX:
      cpu_decode_izpx(DAT_EOR_MDR_A);
      break;
    case INST_EOR_ZP:
      cpu_decode_zp(DAT_EOR_MDR_A);
      break;
    case INST_EOR_IMM:
      cpu_decode_imm(DAT_EOR_MDR_A);
      break;
    case INST_EOR_ABS:
      cpu_decode_abs(DAT_EOR_MDR_A);
      break;
    case INST_EOR_IZP_Y:
      cpu_decode_izp_y(DAT_EOR_MDR_A);
      break;
    case INST_EOR_ZPX:
      cpu_decode_zpx(DAT_EOR_MDR_A);
      break;
    case INST_EOR_ABY:
      cpu_decode_aby(DAT_EOR_MDR_A);
      break;
    case INST_EOR_ABX:
      cpu_decode_abx(DAT_EOR_MDR_A);
      break;
    case INST_ADC_IZPX:
      cpu_decode_izpx(DAT_ADC_MDR_A);
      break;
    case INST_ADC_ZP:
      cpu_decode_zp(DAT_ADC_MDR_A);
      break;
    case INST_ADC_IMM:
      cpu_decode_imm(DAT_ADC_MDR_A);
      break;
    case INST_ADC_ABS:
      cpu_decode_abs(DAT_ADC_MDR_A);
      break;
    case INST_ADC_IZP_Y:
      cpu_decode_izp_y(DAT_ADC_MDR_A);
      break;
    case INST_ADC_ZPX:
      cpu_decode_zpx(DAT_ADC_MDR_A);
      break;
    case INST_ADC_ABY:
      cpu_decode_aby(DAT_ADC_MDR_A);
      break;
    case INST_ADC_ABX:
      cpu_decode_abx(DAT_ADC_MDR_A);
      break;
    case INST_STA_IZPX:
      cpu_decode_w_izpx(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_ZP:
      cpu_decode_w_zp(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_ABS:
      cpu_decode_w_abs(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_IZP_Y:
      cpu_decode_w_izp_y(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_ZPX:
      cpu_decode_w_zpx(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_ABY:
      cpu_decode_w_aby(MEM_WRITE_A_ADDR);
      break;
    case INST_STA_ABX:
      cpu_decode_w_abx(MEM_WRITE_A_ADDR);
      break;
    case INST_LDA_IZPX:
      cpu_decode_izpx(DAT_MOV_MDR_A);
      break;
    case INST_LDA_ZP:
      cpu_decode_zp(DAT_MOV_MDR_A);
      break;
    case INST_LDA_IMM:
      cpu_decode_imm(DAT_MOV_MDR_A);
      break;
    case INST_LDA_ABS:
      cpu_decode_abs(DAT_MOV_MDR_A);
      break;
    case INST_LDA_IZP_Y:
      cpu_decode_izp_y(DAT_MOV_MDR_A);
      break;
    case INST_LDA_ZPX:
      cpu_decode_zpx(DAT_MOV_MDR_A);
      break;
    case INST_LDA_ABY:
      cpu_decode_aby(DAT_MOV_MDR_A);
      break;
    case INST_LDA_ABX:
      cpu_decode_abx(DAT_MOV_MDR_A);
      break;
    case INST_CMP_IZPX:
      cpu_decode_izpx(DAT_CMP_MDR_A);
      break;
    case INST_CMP_ZP:
      cpu_decode_zp(DAT_CMP_MDR_A);
      break;
    case INST_CMP_IMM:
      cpu_decode_imm(DAT_CMP_MDR_A);
      break;
    case INST_CMP_ABS:
      cpu_decode_abs(DAT_CMP_MDR_A);
      break;
    case INST_CMP_IZP_Y:
      cpu_decode_izp_y(DAT_CMP_MDR_A);
      break;
    case INST_CMP_ZPX:
      cpu_decode_zpx(DAT_CMP_MDR_A);
      break;
    case INST_CMP_ABY:
      cpu_decode_aby(DAT_CMP_MDR_A);
      break;
    case INST_CMP_ABX:
      cpu_decode_abx(DAT_CMP_MDR_A);
      break;
    case INST_SBC_IZPX:
      cpu_decode_izpx(DAT_SBC_MDR_A);
      break;
    case INST_SBC_ZP:
      cpu_decode_zp(DAT_SBC_MDR_A);
      break;
    case INST_SBC_IMM:
      cpu_decode_imm(DAT_SBC_MDR_A);
      break;
    case INST_SBC_ABS:
      cpu_decode_abs(DAT_SBC_MDR_A);
      break;
    case INST_SBC_IZP_Y:
      cpu_decode_izp_y(DAT_SBC_MDR_A);
      break;
    case INST_SBC_ZPX:
      cpu_decode_zpx(DAT_SBC_MDR_A);
      break;
    case INST_SBC_ABY:
      cpu_decode_aby(DAT_SBC_MDR_A);
      break;
    case INST_SBC_ABX:
      cpu_decode_abx(DAT_SBC_MDR_A);
      break;
    case INST_ASL_ZP:
      cpu_decode_rw_zp(DAT_ASL_MDR);
      break;
    case INST_ASL_ACC:
      cpu_decode_nomem(DAT_ASL_A);
      break;
    case INST_ASL_ABS:
      cpu_decode_rw_abs(DAT_ASL_MDR);
      break;
    case INST_ASL_ZPX:
      cpu_decode_rw_zpx(DAT_ASL_MDR);
      break;
    case INST_ASL_ABX:
      cpu_decode_rw_abx(DAT_ASL_MDR);
      break;
    case INST_ROL_ZP:
      cpu_decode_rw_zp(DAT_ROL_MDR);
      break;
    case INST_ROL_ACC:
      cpu_decode_nomem(DAT_ROL_A);
      break;
    case INST_ROL_ABS:
      cpu_decode_rw_abs(DAT_ROL_MDR);
      break;
    case INST_ROL_ZPX:
      cpu_decode_rw_zpx(DAT_ROL_MDR);
      break;
    case INST_ROL_ABX:
      cpu_decode_rw_abx(DAT_ROL_MDR);
      break;
    case INST_LSR_ZP:
      cpu_decode_rw_zp(DAT_LSR_MDR);
      break;
    case INST_LSR_ACC:
      cpu_decode_nomem(DAT_LSR_A);
      break;
    case INST_LSR_ABS:
      cpu_decode_rw_abs(DAT_LSR_MDR);
      break;
    case INST_LSR_ZPX:
      cpu_decode_rw_zpx(DAT_LSR_MDR);
      break;
    case INST_LSR_ABX:
      cpu_decode_rw_abx(DAT_LSR_MDR);
      break;
    case INST_ROR_ZP:
      cpu_decode_rw_zp(DAT_ROR_MDR);
      break;
    case INST_ROR_ACC:
      cpu_decode_nomem(DAT_ROR_A);
      break;
    case INST_ROR_ABS:
      cpu_decode_rw_abs(DAT_ROR_MDR);
      break;
    case INST_ROR_ZPX:
      cpu_decode_rw_zpx(DAT_ROR_MDR);
      break;
    case INST_ROR_ABX:
      cpu_decode_rw_abx(DAT_ROR_MDR);
      break;
    case INST_STX_ZP:
      cpu_decode_w_zp(MEM_WRITE_X_ADDR);
      break;
    case INST_STX_ABS:
      cpu_decode_w_abs(MEM_WRITE_X_ADDR);
      break;
    case INST_STX_ZPY:
      cpu_decode_w_zpy(MEM_WRITE_X_ADDR);
      break;
    case INST_LDX_IMM:
      cpu_decode_imm(DAT_MOV_MDR_X);
      break;
    case INST_LDX_ZP:
      cpu_decode_zp(DAT_MOV_MDR_X);
      break;
    case INST_LDX_ABS:
      cpu_decode_abs(DAT_MOV_MDR_X);
      break;
    case INST_LDX_ZPY:
      cpu_decode_zpy(DAT_MOV_MDR_X);
      break;
    case INST_LDX_ABY:
      cpu_decode_aby(DAT_MOV_MDR_X);
      break;
    case INST_DEC_ZP:
      cpu_decode_rw_zp(DAT_DEC_MDR);
      break;
    case INST_DEC_ABS:
      cpu_decode_rw_abs(DAT_DEC_MDR);
      break;
    case INST_DEC_ZPX:
      cpu_decode_rw_zpx(DAT_DEC_MDR);
      break;
    case INST_DEC_ABX:
      cpu_decode_rw_abx(DAT_DEC_MDR);
      break;
    case INST_INC_ZP:
      cpu_decode_rw_zp(DAT_INC_MDR);
      break;
    case INST_INC_ABS:
      cpu_decode_rw_abs(DAT_INC_MDR);
      break;
    case INST_INC_ZPX:
      cpu_decode_rw_zpx(DAT_INC_MDR);
      break;
    case INST_INC_ABX:
      cpu_decode_rw_abx(DAT_INC_MDR);
      break;
    case INST_BIT_ZP:
      cpu_decode_zp(DAT_BIT_MDR_A);
      break;
    case INST_BIT_ABS:
      cpu_decode_abs(DAT_BIT_MDR_A);
      break;
    case INST_JMP:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true);
      state_add_cycle(MEM_READ_PC_PCH, DAT_MOV_MDR_PCL, false);
      state_add_cycle(MEM_FETCH, DAT_NOP, true);
      break;
    case INST_JMPI:
      state_add_cycle(MEM_READ_PC_PTRL, DAT_NOP, true);
      state_add_cycle(MEM_READ_PC_PTRH, DAT_NOP, true);
      state_add_cycle(MEM_READ_PTR_MDR, DAT_NOP, false);
      state_add_cycle(MEM_READ_PTR1_PCH, DAT_MOV_MDR_PCL, false);
      state_add_cycle(MEM_FETCH, DAT_NOP, true);
      break;
    case INST_STY_ZP:
      cpu_decode_w_zp(MEM_WRITE_Y_ADDR);
      break;
    case INST_STY_ABS:
      cpu_decode_w_abs(MEM_WRITE_Y_ADDR);
      break;
    case INST_STY_ZPX:
      cpu_decode_w_zpx(MEM_WRITE_Y_ADDR);
      break;
    case INST_LDY_IMM:
      cpu_decode_imm(DAT_MOV_MDR_Y);
      break;
    case INST_LDY_ZP:
      cpu_decode_zp(DAT_MOV_MDR_Y);
      break;
    case INST_LDY_ABS:
      cpu_decode_abs(DAT_MOV_MDR_Y);
      break;
    case INST_LDY_ZPX:
      cpu_decode_zpx(DAT_MOV_MDR_Y);
      break;
    case INST_LDY_ABX:
      cpu_decode_abx(DAT_MOV_MDR_Y);
      break;
    case INST_CPY_IMM:
      cpu_decode_imm(DAT_CMP_MDR_Y);
      break;
    case INST_CPY_ZP:
      cpu_decode_zp(DAT_CMP_MDR_Y);
      break;
    case INST_CPY_ABS:
      cpu_decode_abs(DAT_CMP_MDR_Y);
      break;
    case INST_CPX_IMM:
      cpu_decode_imm(DAT_CMP_MDR_X);
      break;
    case INST_CPX_ZP:
      cpu_decode_zp(DAT_CMP_MDR_X);
      break;
    case INST_CPX_ABS:
      cpu_decode_abs(DAT_CMP_MDR_X);
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
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true);
      state_add_cycle(MEM_NOP, DAT_BRANCH, false);
      break;
    case INST_BRK:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, PC_NOP);
      state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, PC_NOP);
      state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, PC_NOP);
      state_add_cycle(MEM_BRK, DAT_DEC_S, PC_NOP);
      break;
    case INST_JSR:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true);
      state_add_cycle(MEM_NOP, DAT_NOP, false);
      state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, false);
      state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, false);
      state_add_cycle(MEM_READ_PC_PCH, DAT_MOV_MDR_PCL, false);
      state_add_cycle(MEM_FETCH, DAT_NOP, true);
      break;
    case INST_RTI:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false);
      state_add_cycle(MEM_NOP, DAT_INC_S, false);
      state_add_cycle(MEM_PULL_P, DAT_INC_S, false);
      state_add_cycle(MEM_PULL_PCL, DAT_INC_S, false);
      state_add_cycle(MEM_PULL_PCH, DAT_NOP, false);
      state_add_cycle(MEM_FETCH, DAT_NOP, true);
      break;
    case INST_RTS:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false);
      state_add_cycle(MEM_NOP, DAT_INC_S, false);
      state_add_cycle(MEM_PULL_PCL, DAT_INC_S, false);
      state_add_cycle(MEM_PULL_PCH, DAT_NOP, false);
      state_add_cycle(MEM_NOP, DAT_NOP, true);
      state_add_cycle(MEM_FETCH, DAT_NOP, true);
      break;
    case INST_PHP:
      cpu_decode_push(MEM_PUSH_P_B);
      break;
    case INST_PHA:
      cpu_decode_push(MEM_PUSH_A);
      break;
    case INST_PLP:
      cpu_decode_pull(MEM_PULL_P);
      break;
    case INST_PLA:
      cpu_decode_pull(MEM_PULL_A);
      break;
    case INST_SEC:
      cpu_decode_nomem(DAT_SEC);
      break;
    case INST_SEI:
      cpu_decode_nomem(DAT_SEI);
      break;
    case INST_SED:
      cpu_decode_nomem(DAT_SED);
      break;
    case INST_CLI:
      cpu_decode_nomem(DAT_CLI);
      break;
    case INST_CLC:
      cpu_decode_nomem(DAT_CLC);
      break;
    case INST_CLD:
      cpu_decode_nomem(DAT_CLD);
      break;
    case INST_CLV:
      cpu_decode_nomem(DAT_CLV);
      break;
    case INST_DEY:
      cpu_decode_nomem(DAT_DEC_Y);
      break;
    case INST_DEX:
      cpu_decode_nomem(DAT_DEC_X);
      break;
    case INST_INY:
      cpu_decode_nomem(DAT_INC_Y);
      break;
    case INST_INX:
      cpu_decode_nomem(DAT_INC_X);
      break;
    case INST_TAY:
      cpu_decode_nomem(DAT_MOV_A_Y);
      break;
    case INST_TYA:
      cpu_decode_nomem(DAT_MOV_Y_A);
      break;
    case INST_TXA:
      cpu_decode_nomem(DAT_MOV_X_A);
      break;
    case INST_TXS:
      cpu_decode_nomem(DAT_MOV_X_S);
      break;
    case INST_TAX:
      cpu_decode_nomem(DAT_MOV_A_X);
      break;
    case INST_TSX:
      cpu_decode_nomem(DAT_MOV_S_X);
      break;
    case INST_NOP:
      cpu_decode_nomem(DAT_NOP);
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
void cpu_decode_izpx(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_ADD_PTRL_X, PC_NOP);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zp(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_imm(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_abs(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_izp_y(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_ADD_ADDRL_Y, PC_NOP);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpx(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, PC_NOP);
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_zpy(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_Y, PC_NOP);
  state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_aby(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_Y, PC_INC);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_abx(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, PC_INC);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIXA_ADDRH, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_nomem(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, micro_op, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zp(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, PC_INC);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, PC_NOP);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, PC_NOP);
  state_add_cycle(MEM_FETCH, DAT_NOP, PC_INC);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abs(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_zpx(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_rw_abx(microdata_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, micro_op, false);
  state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izpx(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, true);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_ADD_PTRL_X, false);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, false);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_NOP, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zp(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abs(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_izp_y(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_PTR, DAT_NOP, true);
  state_add_cycle(MEM_READ_PTR_ADDRL, DAT_NOP, false);
  state_add_cycle(MEM_READ_PTR1_ADDRH, DAT_ADD_ADDRL_Y, false);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpx(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_zpy(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_Y, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_aby(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_Y, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_w_abx(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true);
  state_add_cycle(MEM_READ_PC_ADDRH, DAT_ADD_ADDRL_X, true);
  state_add_cycle(MEM_READ_ADDR_MDR, DAT_FIX_ADDRH, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_push(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false);
  state_add_cycle(micro_op, DAT_DEC_S, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
  return;
}

/*
 * TODO
 */
void cpu_decode_pull(micromem_t micro_op) {
  state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false);
  state_add_cycle(MEM_NOP, DAT_INC_S, false);
  state_add_cycle(micro_op, DAT_NOP, false);
  state_add_cycle(MEM_FETCH, DAT_NOP, true);
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
