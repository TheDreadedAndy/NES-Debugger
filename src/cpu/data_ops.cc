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
 * flags. Note that the only memory operation (found in micromem.c) that
 * modifies the processor state is mem_pull_a (since PLA sets the N and Z
 * flags).
 */

#include "./cpu.h"

#include <cstdlib>
#include <cstdio>

#include "../util/data.h"
#include "./cpu_state.h"
#include "./cpu_status.h"

/*
 * Does nothing.
 */
void Cpu::Nop(void) {
  // What did you expect?
  return;
}

/*
 * Increments the stack pointer.
 */
void Cpu::DataIncS(void) {
  regs_->s.w[WORD_LO]++;
  return;
}

/*
 * Increments the X register. Sets the N and Z flags.
 */
void Cpu::DataIncX(void) {
  regs_->x++;
  regs_->p.negative = (regs_->x & STATUS_FLAG_N);
  regs_->p.zero = (regs_->x == 0);
  return;
}

/*
 * Increments the Y register. Sets the N and Z flags.
 */
void Cpu::DataIncY(void) {
  regs_->y++;
  regs_->p.negative = (regs_->y & STATUS_FLAG_N);
  regs_->p.zero = (regs_->y == 0);
  return;
}

/*
 * Increments the MDR. Sets the N and Z flags.
 */
void Cpu::DataIncMdr(void) {
  regs_->mdr++;
  regs_->p.negative = (regs_->mdr & STATUS_FLAG_N);
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Decrements the S register. Used in push/pull. Does not set flags.
 */
void Cpu::DataDecS(void) {
  regs_->s.w[WORD_LO]--;
  return;
}

/*
 * Decrements the X register. Sets the N and Z flags.
 */
void Cpu::DataDecX(void) {
  regs_->x--;
  regs_->p.negative = (regs_->x & STATUS_FLAG_N);
  regs_->p.zero = (regs_->x == 0);
  return;
}

/*
 * Decrements the Y register. Sets the N and Z flags.
 */
void Cpu::DataDecY(void) {
  regs_->y--;
  regs_->p.negative = (regs_->y & STATUS_FLAG_N);
  regs_->p.zero = (regs_->y == 0);
  return;
}

/*
 * Decrements the MDR. Sets the N and Z flags.
 */
void Cpu::DataDecMdr(void) {
  regs_->mdr--;
  regs_->p.negative = (regs_->mdr & STATUS_FLAG_N);
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Copies the value stored in A to X. Sets the N and Z flags.
 */
void Cpu::DataMovAX(void) {
  regs_->x = regs_->a;
  regs_->p.negative = (regs_->x & STATUS_FLAG_N);
  regs_->p.zero = (regs_->x == 0);
  return;
}

/*
 * Copies the value stored in A to Y. Sets the N and Z flags.
 */
void Cpu::DataMovAY(void) {
  regs_->y = regs_->a;
  regs_->p.negative = (regs_->y & STATUS_FLAG_N);
  regs_->p.zero = (regs_->y == 0);
  return;
}

/*
 * Copies the value stored in S to X. Sets the N and Z flags.
 */
void Cpu::DataMovSX(void) {
  regs_->x = regs_->s.w[WORD_LO];
  regs_->p.negative = (regs_->x & STATUS_FLAG_N);
  regs_->p.zero = (regs_->x == 0);
  return;
}

/*
 * Copies the value stored in X to A. Sets the N and Z flags.
 */
void Cpu::DataMovXA(void) {
  regs_->a = regs_->x;
  regs_->p.negative = (regs_->a & STATUS_FLAG_N);
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Copies the value stored in X to S. Sets no flag (S is the stack pointer).
 */
void Cpu::DataMovXS(void) {
  regs_->s.w[WORD_LO] = regs_->x;
  return;
}

/*
 * Copies the value stored in Y to A. Sets the N and Z flags.
 */
void Cpu::DataMovYA(void) {
  regs_->a = regs_->y;
  regs_->p.negative = (regs_->a & STATUS_FLAG_N);
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Copies the value stored in the MDR to the PCL register. Sets no flags.
 */
void Cpu::DataMovMdrPcl(void) {
  regs_->pc.w[WORD_LO] = regs_->mdr;
  return;
}

/*
 * Copies the value stored in the MDR to Register A. Sets the N and Z flags.
 */
void Cpu::DataMovMdrA(void) {
  regs_->a = regs_->mdr;
  regs_->p.negative = (regs_->a & STATUS_FLAG_N);
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Copies the value stored in the MDR to Register X. Sets the N and Z flags.
 */
void Cpu::DataMovMdrX(void) {
  regs_->x = regs_->mdr;
  regs_->p.negative = (regs_->x & STATUS_FLAG_N);
  regs_->p.zero = (regs_->x == 0);
  return;
}

/*
 * Copies the value stored in the MDR to Register Y. Sets the N and Z flags.
 */
void Cpu::DataMovMdrY(void) {
  regs_->y = regs_->mdr;
  regs_->p.negative = (regs_->y & STATUS_FLAG_N);
  regs_->p.zero = (regs_->y == 0);
  return;
}

/*
 * Clears the carry flag.
 */
void Cpu::DataClc(void) {
  regs_->p.carry = false;
  return;
}

/*
 * Clears the decimal flag.
 */
void Cpu::DataCld(void) {
  regs_->p.decimal = false;
  return;
}

/*
 * Clears the interrupt flag.
 */
void Cpu::DataCli(void) {
  regs_->p.irq_disable = false;
  return;
}

/*
 * Clears the overflow flag.
 */
void Cpu::DataClv(void) {
  regs_->p.overflow = false;
  return;
}

/*
 * Sets the carry flag.
 */
void Cpu::DataSec(void) {
  regs_->p.carry = true;
  return;
}

/*
 * Sets the decimal flag.
 */
void Cpu::DataSed(void) {
  regs_->p.decimal = true;
  return;
}

/*
 * Sets the interrupt flag.
 */
void Cpu::DataSei(void) {
  regs_->p.irq_disable = true;
  return;
}

/*
 * Subtracts the MDR from Register A, and stores the N, Z, and C flags
 * of the result.
 */
void Cpu::DataCmpMdrA(void) {
  regs_->p.negative = ((regs_->a - regs_->mdr) & STATUS_FLAG_N);
  regs_->p.carry = (regs_->a >= regs_->mdr);
  regs_->p.zero = (regs_->a == regs_->mdr);
  return;
}

/*
 * Subtracts the MDR from Register X, and stores the N, Z, and C flags
 * of the result.
 */
void Cpu::Data_cmp_mdr_x(void) {
  regs_->p = (regs_->p & 0x7CU) | ((regs_->x - regs_->mdr) & 0x80U) | (regs_->x >= regs_->mdr)
                        | ((regs_->x == regs_->mdr) << 1U);
  return;
}

/*
 * Subtracts the MDR from Register Y, and stores the N, Z, and C flags
 * of the result.
 */
void Cpu::Data_cmp_mdr_y(void) {
  regs_->p = (regs_->p & 0x7CU) | ((regs_->y - regs_->mdr) & 0x80U) | (regs_->y >= regs_->mdr)
                        | ((regs_->y == regs_->mdr) << 1U);
  return;
}

/*
 * Shifts the MDR left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::Data_asl_mdr(void) {
  word_t carry = (regs_->mdr >> 7U) & 0x01U;
  regs_->mdr = (regs_->mdr << 1U) & 0xFEU;
  regs_->p = (regs_->p & 0x7CU) | (regs_->mdr & 0x80U) | ((regs_->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::Data_asl_a(void) {
  word_t carry = (regs_->a >> 7U) & 0x01U;
  regs_->a = (regs_->a << 1U) & 0xFEU;
  regs_->p = (regs_->p & 0x7CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::Data_lsr_mdr(void) {
  word_t carry = regs_->mdr & 0x01U;
  regs_->mdr = (regs_->mdr >> 1U) & 0x7FU;
  regs_->p = (regs_->p & 0x7CU) | (regs_->mdr & 0x80U) | ((regs_->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::Data_lsr_a(void) {
  word_t carry = regs_->a & 0x01U;
  regs_->a = (regs_->a >> 1U) & 0x7FU;
  regs_->p = (regs_->p & 0x7CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR left once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::Data_rol_mdr(void) {
  word_t carry = (regs_->mdr >> 7U) & 0x01U;
  regs_->mdr = ((regs_->mdr << 1U) & 0xFEU) | (regs_->p & 0x01U);
  regs_->p = (regs_->p & 0x7CU) | (regs_->mdr & 0x80U) | ((regs_->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A left once, back filling with C. Stores the lost bit in C, sets
 * the N and Z flags.
 */
void Cpu::Data_rol_a(void) {
  word_t carry = (regs_->a >> 7U) & 0x01U;
  regs_->a = ((regs_->a << 1U) & 0xFEU) | (regs_->p & 0x01U);
  regs_->p = (regs_->p & 0x7CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts the MDR right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::Data_ror_mdr(void) {
  word_t carry = regs_->mdr & 0x01U;
  regs_->mdr = ((regs_->mdr >> 1U) & 0x7FU) | ((regs_->p << 7U) & 0x80U);
  regs_->p = (regs_->p & 0x7CU) | (regs_->mdr & 0x80U) | ((regs_->mdr == 0U) << 1U) | carry;
  return;
}

/*
 * Shifts A right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::Data_ror_a(void) {
  word_t carry = regs_->a & 0x01U;
  regs_->a = ((regs_->a >> 1U) & 0x7FU) | ((regs_->p << 7U) & 0x80U);
  regs_->p = (regs_->p & 0x7CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U) | carry;
  return;
}

/*
 * XOR's the MDR and A. Sets the N and Z flags.
 */
void Cpu::Data_eor_mdr_a(void) {
  regs_->a = regs_->a ^ regs_->mdr;
  regs_->p = (regs_->p & 0x7DU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U);
  return;
}

/*
 * AND's the MDR and A. Sets the N and Z flags.
 */
void Cpu::Data_and_mdr_a(void) {
  regs_->a = regs_->a & regs_->mdr;
  regs_->p = (regs_->p & 0x7DU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U);
  return;
}

/*
 * OR's the MDR and A. Sets the N and Z flags.
 */
void Cpu::Data_ora_mdr_a(void) {
  regs_->a = regs_->a | regs_->mdr;
  regs_->p = (regs_->p & 0x7DU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U);
  return;
}

/*
 * Adds the MDR, A, and the C flag, storing the result in A.
 * Sets the N, V, Z, and C flags.
 */
void Cpu::Data_adc_mdr_a(void) {
  mword_t res;
  res.dw = regs_->a + regs_->mdr + (regs_->p & 0x01U);
  word_t ovf = (((~(regs_->a ^ regs_->mdr)) & (regs_->a ^ res.w[WORD_LO])) & 0x80) >> 1U;
  regs_->a = res.w[WORD_LO];
  regs_->p = (regs_->p & 0x3CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U)
                        | ovf | res.w[WORD_HI];
  return;
}

/*
 * Subtracts the MDR from A, using C as a borrow flag. The result
 * is equal to A - MDR - (1 - C). Sets the N, V, Z, and C flags.
 */
void Cpu::Data_sbc_mdr_a(void) {
  // See documentation for proof of this line. Gives the correct result
  // without issues in the carry out.
  mword_t res;
  res.dw = regs_->a + ((~regs_->mdr) & WORD_MASK) + (regs_->p & 0x01U);
  word_t ovf = (((regs_->a ^ regs_->mdr) & (regs_->a ^ res.w[WORD_LO])) & 0x80) >> 1U;
  regs_->a = res.w[WORD_LO];
  regs_->p = (regs_->p & 0x3CU) | (regs_->a & 0x80U) | ((regs_->a == 0U) << 1U)
                        | ovf | res.w[WORD_HI];
  return;
}

/*
 * Moves the two high bits of the MDR to the state register (N and V).
 * Sets the zero flag according to A AND MDR.
 */
void Cpu::Data_bit_mdr_a(void) {
  regs_->p = (regs_->p & 0x3DU) | (regs_->mdr & 0xC0U) | (((regs_->a & regs_->mdr) == 0U) << 1U);
  return;
}

/*
 * Adds X to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void Cpu::Data_add_addrl_x(void) {
  dword_t res = ((dword_t) (regs_->addr.w[WORD_LO])) + ((dword_t) regs_->x);
  regs_->addr.w[WORD_LO] = (word_t) res;
  regs_->addr_carry = res >> 8U;
  return;
}

/*
 * Adds Y to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void Cpu::Data_add_addrl_y(void) {
  dword_t res = ((dword_t) (regs_->addr.w[WORD_LO])) + ((dword_t) regs_->y);
  regs_->addr.w[WORD_LO] = (word_t) res;
  regs_->addr_carry = res >> 8U;
  return;
}

/*
 * Adds X to the low pointer byte. Page crossings are ignored.
 */
void Cpu::Data_add_ptrl_x(void) {
  regs_->ptr.w[WORD_LO] = regs_->ptr.w[WORD_LO] + regs_->x;
  return;
}

/*
 * Performs the last addressing operation again if the address crossed a
 * page bound and needed to be fixed.
 */
void Cpu::Data_fixa_addrh(void) {
  if (regs_->addr_carry > 0) {
    regs_->addr.w[WORD_HI] += regs_->addr_carry;
    micro_t *micro = state_last_cycle();
    state_push_cycle(micro->mem, &data_nop, PC_NOP);
  }
  return;
}

/*
 * Adds the carry out from the last addressing data operation to addr_hi.
 */
void Cpu::Data_fix_addrh(void) {
  regs_->addr.w[WORD_HI] = regs_->addr.w[WORD_HI] + regs_->addr_carry;
  return;
}

/*
 * Adds the carry out from the last addressing data operation to PCH.
 */
void Cpu::Data_fix_pch(void) {
  regs_->pc.w[WORD_HI] = regs_->pc.w[WORD_HI] + regs_->addr_carry;
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
void Cpu::Data_branch(void) {
  // Calculate whether or not the branch was taken.
  word_t flag = (regs_->inst >> 6U) & 0x03U;
  bool cond = (bool) ((regs_->inst >> 5U) & 1U);
  // Black magic that pulls the proper flag from the status reg.
  flag = (flag & 2U) ? ((regs_->p >> (flag & 1U)) & 1U)
                     : ((regs_->p >> ((~flag) & 0x07U)) & 1U);
  bool taken = (((bool) flag) == cond);

  // Add the reletive address to pc_lo. Reletive addressing is signed.
  dword_t res = regs_->pc.w[WORD_LO] + regs_->mdr;
  regs_->addr_carry = res >> 8U;
  // Effectively sign extend the MDR in the carry out.
  if (regs_->mdr & 0x80U) { regs_->addr_carry += 0xFFU; }

  // Execute the proper cycles according to the above results.
  if (!taken) {
    // Case 3.
    cpu_fetch(state_last_cycle());
  } else if (regs_->addr_carry) {
    // Case 2.
    regs_->pc.w[WORD_LO] = res;
    state_add_cycle(&mem_nop, &data_fix_pch, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  } else {
    // Case 1.
    regs_->pc.w[WORD_LO] = res;
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  }

  return;
}
