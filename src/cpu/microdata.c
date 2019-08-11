/*
 * This file contains functtions which implement micro operations for the 6502.
 * These micro ops are an abstraction and do not exist within the original
 * processor.
 *
 * These operations are called from the 2A03 emulation using the state queue.
 * These operations assume that the regfile has been initialized.
 *
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

#include <stdlib.h>
#include <stdio.h>
#include "../util/data.h"
#include "../util/contracts.h"
#include "./2A03.h"
#include "./regs.h"
#include "./micromem.h"
#include "./state.h"

/*
 * Does nothing.
 */
void data_nop(void) {
  // What did you expect?
  return;
}

/*
 * Increments the stack pointer.
 */
void data_inc_s(void) {
  R->S++;
  return;
}

/*
 * Increments the X register. Sets the N and Z flags.
 */
void data_inc_x(void) {
  R->X++;
  R->P = (R->P & 0x7DU) | (R->X & 0x80U) | ((R->X == 0U) << 1U);
  return;
}

/*
 * Increments the Y register. Sets the N and Z flags.
 */
void data_inc_y(void) {
  R->Y++;
  R->P = (R->P & 0x7DU) | (R->Y & 0x80U) | ((R->Y == 0U) << 1U);
  return;
}

/*
 * Increments the MDR. Sets the N and Z flags.
 */
void data_inc_mdr(void) {
  R->mdr++;
  R->P = (R->P & 0x7DU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U);
  return;
}

/*
 * Decrements the S register. Used in push/pull. Does not set flags.
 */
void data_dec_s(void) {
  R->S--;
  return;
}

/*
 * Decrements the X register. Sets the N and Z flags.
 */
void data_dec_x(void) {
  R->X--;
  R->P = (R->P & 0x7DU) | (R->X & 0x80U) | ((R->X == 0U) << 1U);
  return;
}

/*
 * Decrements the Y register. Sets the N and Z flags.
 */
void data_dec_y(void) {
  R->Y--;
  R->P = (R->P & 0x7DU) | (R->Y & 0x80U) | ((R->Y == 0U) << 1U);
  return;
}

/*
 * Decrements the MDR. Sets the N and Z flags.
 */
void data_dec_mdr(void) {
  R->mdr--;
  R->P = (R->P & 0x7DU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in A to X. Sets the N and Z flags.
 */
void data_mov_a_x(void) {
  R->X = R->A;
  R->P = (R->P & 0x7DU) | (R->X & 0x80U) | ((R->X == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in A to Y. Sets the N and Z flags.
 */
void data_mov_a_y(void) {
  R->Y = R->A;
  R->P = (R->P & 0x7DU) | (R->Y & 0x80U) | ((R->Y == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in S to X. Sets the N and Z flags.
 */
void data_mov_s_x(void) {
  R->X = R->S;
  R->P = (R->P & 0x7DU) | (R->X & 0x80U) | ((R->X == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in X to A. Sets the N and Z flags.
 */
void data_mov_x_a(void) {
  R->A = R->X;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in X to S. Sets no flag (S is the stack pointer).
 */
void data_mov_x_s(void) {
  R->S = R->X;
  return;
}

/*
 * Copies the value stored in Y to A. Sets the N and Z flags.
 */
void data_mov_y_a(void) {
  R->A = R->Y;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in the MDR to the PCL register. Sets no flags.
 */
void data_mov_mdr_pcl(void) {
  R->pc_lo = R->mdr;
  return;
}

/*
 * Copies the value stored in the MDR to Register A. Sets the N and Z flags.
 */
void data_mov_mdr_a(void) {
  R->A = R->mdr;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in the MDR to Register X. Sets the N and Z flags.
 */
void data_mov_mdr_x(void) {
  R->X = R->mdr;
  R->P = (R->P & 0x7DU) | (R->X & 0x80U) | ((R->X == 0U) << 1U);
  return;
}

/*
 * Copies the value stored in the MDR to Register X. Sets the N and Z flags.
 */
void data_mov_mdr_y(void) {
  R->Y = R->mdr;
  R->P = (R->P & 0x7DU) | (R->Y & 0x80U) | ((R->Y == 0U) << 1U);
  return;
}

/*
 * Clears the carry flag.
 */
void data_clc(void) {
  R->P = R->P & 0xFEU;
  return;
}

/*
 * Clears the decimal flag.
 */
void data_cld(void) {
  R->P = R->P & 0xF7U;
  return;
}

/*
 * Clears the interrupt flag.
 */
void data_cli(void) {
  R->P = R->P & 0xFBU;
  return;
}

/*
 * Clears the overflow flag.
 */
void data_clv(void) {
  R->P = R->P & 0xBFU;
  return;
}

/*
 * Sets the carry flag.
 */
void data_sec(void) {
  R->P = R->P | 0x01U;
  return;
}

/*
 * Sets the decimal flag.
 */
void data_sed(void) {
  R->P = R->P | 0x08U;
  return;
}

/*
 * Sets the interrupt flag.
 */
void data_sei(void) {
  R->P = R->P | 0x04U;
  return;
}

/*
 * Subtracts the MDR from Register A, and stores the N, Z, and C flags
 * of the result.
 */
void data_cmp_mdr_a(void) {
  // The carry flag is unsigned overflow. We use a double word to hold
  // the extra bit.
  dword_t res = ((dword_t) R->A) + (((dword_t) (-R->mdr)) & 0xFFU);
  R->P = (R->P & 0x7CU) | (res & 0x80U) | (res >> 8U)
                        | (((res & 0xFF) == 0U) << 1U);
  return;
}

/*
 * Subtracts the MDR from Register X, and stores the N, Z, and C flags
 * of the result.
 */
void data_cmp_mdr_x(void) {
  dword_t res = ((dword_t) R->X) + (((dword_t) (-R->mdr)) & 0xFFU);
  R->P = (R->P & 0x7CU) | (res & 0x80U) | (res >> 8U)
                        | (((res & 0xFF) == 0U) << 1U);
  return;
}

/*
 * Subtracts the MDR from Register Y, and stores the N, Z, and C flags
 * of the result.
 */
void data_cmp_mdr_y(void) {
  dword_t res = ((dword_t) R->Y) + (((dword_t) (-R->mdr)) & 0xFFU);
  R->P = (R->P & 0x7CU) | (res & 0x80U) | (res >> 8U)
                        | (((res & 0xFF) == 0U) << 1U);
  return;
}

/*
 * Shifts the MDR left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void data_asl_mdr(void) {
  word_t carry = R->mdr >> 7U;
  R->mdr = R->mdr << 1U;
  R->P = (R->P & 0x7CU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void data_asl_a(void) {
  word_t carry = R->A >> 7U;
  R->A = R->A << 1U;
  R->P = (R->P & 0x7CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void data_lsr_mdr(void) {
  word_t carry = R->mdr & 0x01U;
  R->mdr = R->mdr >> 1U;
  R->P = (R->P & 0x7CU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void data_lsr_a(void) {
  word_t carry = R->A & 0x01U;
  R->A = R->A >> 1U;
  R->P = (R->P & 0x7CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR left once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void data_rol_mdr(void) {
  word_t carry = R->mdr >> 7U;
  R->mdr = (R->mdr << 1U) | (R->P & 0x01U);
  R->P = (R->P & 0x7CU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A left once, back filling with C. Stores teh lost bit in C, sets
 * the N and Z flags.
 */
void data_rol_a(void) {
  word_t carry = R->A >> 7U;
  R->A = (R->A << 1U) | (R->P & 0x01U);
  R->P = (R->P & 0x7CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void data_ror_mdr(void) {
  word_t carry = R->mdr & 0x01U;
  R->mdr = (R->mdr >> 1U) | (R->P << 7U);
  R->P = (R->P & 0x7CU) | (R->mdr & 0x80U) | ((R->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void data_ror_a(void) {
  word_t carry = R->A & 0x01U;
  R->A = (R->A >> 1U) | (R->P << 7U);
  R->P = (R->P & 0x7CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U) | carry;
  return;
}

/*
 * XOR's the MDR and A. Sets the N and Z flags.
 */
void data_eor_mdr_a(void) {
  R->A = R->A ^ R->mdr;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * AND's the MDR and A. Sets the N and Z flags.
 */
void data_and_mdr_a(void) {
  R->A = R->A & R->mdr;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * OR's the MDR and A. Sets the N and Z flags.
 */
void data_ora_mdr_a(void) {
  R->A = R->A | R->mdr;
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * Adds the MDR, A, and the C flag, storing the result in A.
 * Sets the N, V, Z, and C flags.
 */
void data_adc_mdr_a(void) {
  dword_t res = ((dword_t) R->A) + ((dword_t) R->mdr)
                                 + ((dword_t) (R->P & 0x01U));
  word_t ovf = ((R->A & 0x80U) == (R->mdr & 0x80U))
            && ((R->A & 0x80U) != (res & 0x80U));
  R->A = (word_t) res;
  R->P = (R->P & 0x3CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U)
                       | (ovf << 6U) | (res >> 8U);
  return;
}

/*
 * Subtracts the MDR from A, using C as a borrow flag. The result
 * is equal to A - MDR - (1 - C). Sets the N, V, Z, and C flags.
 */
void data_sbc_mdr_a(void) {
  // See documentation for proof of this line. Gives the correct result
  // without issues in the carry out.
  dword_t res = ((dword_t) R->A) + (((dword_t) (~R->mdr)) & 0xFFU)
                                 + ((dword_t) (R->P & 0x01U));
  word_t ovf = ((R->A & 0x80U) == ((~R->mdr) & 0x80U))
            && ((R->A & 0x80U) != (res & 0x80U));
  R->A = (word_t) res;
  R->P = (R->P & 0x3CU) | (R->A & 0x80U) | ((R->A == 0U) << 1U)
                        | (ovf << 6U) | (res >> 8U);
  return;
}

/*
 * Moves the two high bits of the MDR to the state register (N and V).
 * Sets the zero flag according to A AND MDR.
 */
void data_bit_mdr_a(void) {
  R->P = (R->P & 0x3DU) | (R->mdr & 0xC0U) | (((R->A & R->mdr) == 0U) << 1U);
  return;
}

/*
 * Adds X to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void data_add_addrl_x(void) {
  dword_t res = ((dword_t) R->addr_lo) + ((dword_t) R->X);
  R->addr_lo = (word_t) res;
  R->carry = res >> 8U;
  return;
}

/*
 * Adds Y to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void data_add_addrl_y(void) {
  dword_t res = ((dword_t) R->addr_lo) + ((dword_t) R->Y);
  R->addr_lo = (word_t) res;
  R->carry = res >> 8U;
  return;
}

/*
 * Adds X to the low pointer byte. Page crossings are ignored.
 */
void data_add_ptrl_x(void) {
  R->ptr_lo = R->ptr_lo + R->X;
  return;
}

/*
 * Performs the last addressing operation again if the address crossed a
 * page bound and needed to be fixed.
 */
void data_fixa_addrh(void) {
  if (R->carry) {
    R->addr_hi += R->carry;
    micro_t *micro = state_last_cycle();
    state_push_cycle(micro->mem, &data_nop, PC_NOP);
  }
  return;
}

/*
 * Adds the carry out from the last addressing data operation to addr_hi.
 */
void data_fix_addrh(void) {
  R->addr_hi = R->addr_hi + R->carry;
  return;
}

/*
 * Adds the carry out from the last addressing data operation to PCH.
 */
void data_fix_pch(void) {
  R->pc_hi = R->pc_hi + R->carry;
  return;
}

/*
 * Branch instructions are of the form xxy10000, and are broken into
 * three cases:
 * 1) If the flag indicated by xx has value y, then the reletive address
 * is added to the PC.
 * 2) If case 1 results in a page crossing on the pc, an extra cycle is
 * added.
 * 3) If xx does not have value y, this micro op is the same as MEM_FETCH.
 *
 * This function implements that behavior.
 */
void data_branch(void) {
  // Calculate whether or not the branch was taken.
  word_t flag = (R->inst >> 6U) & 3U;
  bool cond = (bool) ((R->inst >> 5U) & 1U);
  // Black magic that pulls the proper flag from the status reg.
  flag = (flag & 2U) ? ((R->P >> (flag & 1U)) & 1U)
                     : ((R->P >> (7U - flag)) & 1U);
  bool taken = (((bool) flag) == cond);

  // Add the reletive address to pc_lo. Reletive addressing is signed.
  dword_t res = ((dword_t) R->pc_lo) + ((dword_t) R->mdr);
  R->carry = (word_t)(res >> 8U);
  // Effectively sign extend the MDR in the carry out.
  if (R->mdr & 0x80U) { R->carry += 0xFFU; }

  // Execute the proper cycles according to the above results.
  if (!taken) {
    // Case 3.
    cpu_fetch(state_last_cycle());
  } else if (R->carry) {
    // Case 2.
    R->pc_lo = (word_t) res;
    state_add_cycle(&mem_nop, &data_fix_pch, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  } else {
    // Case 1.
    R->pc_lo = (word_t) res;
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  }

  return;
}
