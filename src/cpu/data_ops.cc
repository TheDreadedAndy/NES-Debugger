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
void Cpu::DataCmpMdrX(void) {
  regs_->p.negative = ((regs_->x - regs_->mdr) & STATUS_FLAG_N);
  regs_->p.carry = (regs_->x >= regs_->mdr);
  regs_->p.zero = (regs_->x == regs_->mdr);
  return;
}

/*
 * Subtracts the MDR from Register Y, and stores the N, Z, and C flags
 * of the result.
 */
void Cpu::DataCmpMdrY(void) {
  regs_->p.negative = ((regs_->y - regs_->mdr) & STATUS_FLAG_N);
  regs_->p.carry = (regs_->y >= regs_->mdr);
  regs_->p.zero = (regs_->y == regs_->mdr);
  return;
}

/*
 * Shifts the MDR left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::DataAslMdr(void) {
  regs_->p.carry = (regs_->mdr >> 7U) & 0x01U;
  regs_->mdr = regs_->mdr << 1U;
  regs_->p.negative = (regs_->mdr & STATUS_FLAG_N);
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Shifts A left once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::DataAslA(void) {
  regs_->p.carry = (regs_->a >> 7U) & 0x01U;
  regs_->a = regs_->a << 1U;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Shifts the MDR right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::DataLsrMdr(void) {
  regs_->p.carry = regs_->mdr & 0x01U;
  regs_->mdr = (regs_->mdr >> 1U) & 0x7FU;
  regs_->p.negative = regs_->mdr & STATUS_FLAG_N;
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Shifts A right once, storing the lost bit in C and setting the
 * N and Z flags.
 */
void Cpu::DataLsrA(void) {
  regs_->p.carry = regs_->a & 0x01U;
  regs_->a = (regs_->a >> 1U) & 0x7FU;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Shifts the MDR left once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::DataRolMdr(void) {
  regs_->p.carry = (regs_->mdr >> 7U) & 0x01U;
  regs_->mdr = ((regs_->mdr << 1U) & 0xFEU) | regs_->p.carry;
  regs_->p.negative = regs_->mdr & STATUS_FLAG_N;
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Shifts A left once, back filling with C. Stores the lost bit in C, sets
 * the N and Z flags.
 */
void Cpu::DataRolA(void) {
  regs_->p.carry = (regs_->a >> 7U) & 0x01U;
  regs_->a = ((regs_->a << 1U) & 0xFEU) | regs_->p.carry;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Shifts the MDR right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::DataRorMdr(void) {
  regs_->p.carry = regs_->mdr & 0x01U;
  regs_->mdr = ((regs_->mdr >> 1U) & 0x7FU) | (regs_->p.carry << 7U);
  regs_->p.negative = regs_->mdr & STATUS_FLAG_N;
  regs_->p.zero = (regs_->mdr == 0);
  return;
}

/*
 * Shifts A right once, back filling with C. Stores the lost bit in C,
 * sets the N and Z flags.
 */
void Cpu::DataRorA(void) {
  regs_->p.carry = regs_->a & 0x01U;
  regs_->a = ((regs_->a >> 1U) & 0x7FU) | (regs_->p.carry << 7U);
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * XOR's the MDR and A. Sets the N and Z flags.
 */
void Cpu::DataEorMdrA(void) {
  regs_->a = regs_->a ^ regs_->mdr;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * AND's the MDR and A. Sets the N and Z flags.
 */
void Cpu::DataAndMdrA(void) {
  regs_->a = regs_->a & regs_->mdr;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * OR's the MDR and A. Sets the N and Z flags.
 */
void Cpu::DataOraMdrA(void) {
  regs_->a = regs_->a | regs_->mdr;
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Adds the MDR, A, and the C flag, storing the result in A.
 * Sets the N, V, Z, and C flags.
 */
void Cpu::DataAdcMdrA(void) {
  MultiWord res;
  res.dw = regs_->a + regs_->mdr + regs_->p.carry;
  regs_->p.overflow = (((~(regs_->a ^ regs_->mdr))
                    & (regs_->a ^ res.w[WORD_LO])) & 0x80);
  regs_->a = res.w[WORD_LO];
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  regs_->p.carry = res.w[WORD_HI];
  return;
}

/*
 * Subtracts the MDR from A, using C as a borrow flag. The result
 * is equal to A - MDR - (1 - C). Sets the N, V, Z, and C flags.
 */
void Cpu::DataSbcMdrA(void) {
  // See documentation for proof of this line. Gives the correct result
  // without issues in the carry out.
  MultiWord res;
  res.dw = regs_->a + ((~regs_->mdr) & WORD_MASK) + regs_->p.carry;
  regs_->p.overflow = (((regs_->a ^ regs_->mdr)
                    & (regs_->a ^ res.w[WORD_LO])) & 0x80);
  regs_->a = res.w[WORD_LO];
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  regs_->p.carry = res.w[WORD_HI];
  return;
}

/*
 * Moves the two high bits of the MDR to the state register (N and V).
 * Sets the zero flag according to A AND MDR.
 */
void Cpu::DataBitMdrA(void) {
  regs_->p.negative = regs_->mdr & STATUS_FLAG_N;
  regs_->p.overflow = regs_->mdr & STATUS_FLAG_V;
  regs_->p.zero = ((regs_->a & regs_->mdr) == 0);
  return;
}

/*
 * Adds X to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void Cpu::DataAddAddrlX(void) {
  MultiWord res;
  res.dw = regs_->addr.w[WORD_LO] + regs_->x;
  regs_->addr.w[WORD_LO] = res.w[WORD_LO];
  regs_->addr_carry = res.w[WORD_HI];
  return;
}

/*
 * Adds Y to the low address byte, storing the carry out in the carry
 * abstraction register.
 */
void Cpu::DataAddAddrlY(void) {
  MultiWord res;
  res.dw = regs_->addr.w[WORD_LO] + regs_->x;
  regs_->addr.w[WORD_LO] = res.w[WORD_LO];
  regs_->addr_carry = res.w[WORD_HI];
  return;
}

/*
 * Adds X to the low pointer byte. Page crossings are ignored.
 */
void Cpu::DataAddPtrlX(void) {
  regs_->ptr.w[WORD_LO] = regs_->ptr.w[WORD_LO] + regs_->x;
  return;
}

/*
 * Performs the last addressing operation again if the address crossed a
 * page bound and needed to be fixed.
 */
void Cpu::DataFixaAddrh(void) {
  if (regs_->addr_carry > 0) {
    regs_->addr.w[WORD_HI] += regs_->addr_carry;
    OperationCycle *op = state_->GetLastCycle();
    state_->PushCycle(op->mem, &Cpu::Nop, PC_NOP);
  }
  return;
}

/*
 * Adds the carry out from the last addressing data operation to addr_hi.
 */
void Cpu::DataFixAddrh(void) {
  regs_->addr.w[WORD_HI] = regs_->addr.w[WORD_HI] + regs_->addr_carry;
  return;
}

/*
 * Adds the carry out from the last addressing data operation to PCH.
 */
void Cpu::DataFixPch(void) {
  regs_->pc.w[WORD_HI] = regs_->pc.w[WORD_HI] + regs_->addr_carry;
  return;
}

/*
 * Branch instructions are of the form xxy10000, and are broken into
 * three cases:
 * 1) If the flag indicated by xx has value y, then the relative address
 * is added to the PC.
 * 2) If case 1 results in a page crossing on the pc, an extra cycle is
 * added.
 * 3) If xx does not have value y, this micro op is the same as MEM_FETCH.
 *
 * This function implements that behavior.
 */
void Cpu::DataBranch(void) {
  // Calculate whether or not the branch was taken.
  DataWord flag = (regs_->inst >> 6U) & 0x03U;
  bool cond = regs_->inst & 0x20U;
  // Black magic that pulls the proper flag from the status reg.
  DataWord status = StatusGetVector(&(regs_->p), false);
  flag = (flag & 2U) ? ((status >> (flag & 1U)) & 1U)
                     : ((status >> ((~flag) & 0x07U)) & 1U);
  bool taken = ((static_cast<bool>(flag)) == cond);

  // Add the reletive address to pc_lo. Reletive addressing is signed.
  MultiWord res;
  res.dw = regs_->pc.w[WORD_LO] + regs_->mdr;
  // Effectively sign extends the MDR in the carry out.
  regs_->addr_carry = (regs_->mdr & 0x80) ? (res.w[WORD_HI] + 0xFFU)
                                          : res.w[WORD_HI];

  // Execute the proper cycles according to the above results.
  if (!taken) {
    // Case 3.
    Fetch(state_->GetLastCycle());
  } else if (regs_->addr_carry) {
    // Case 2.
    regs_->pc.w[WORD_LO] = res.w[WORD_LO];
    state_->AddCycle(&Cpu::Nop, &Cpu::DataFixPch, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  } else {
    // Case 1.
    regs_->pc.w[WORD_LO] = res.w[WORD_LO];
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  }

  return;
}
