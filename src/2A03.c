#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "2A03.h"
#include "util.h"
#include "contracts.h"
#include "memory.h"
#include "machinecode.h"
#include "state.h"
#include "microops.h"

/* The microinstructions of the 6502 are handled by the state_t structure.
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

// Interrupt bools.
bool IRQ, NMI;

// Helper functions
void cpu_run_mem(micro_t *micro, regfile_t *R, memory_t *M, state_t *S);
void cpu_run_data(micro_t *micro, regfile_t *R, memory_t *M, state_t *S);
bool cpu_can_poll(microdata_t dat, regfile_t *R, state_t *S);
void cpu_fetch_inst(uint8_t inst, bool NMIInt, bool IRQInt, state_t *S);

// Run the next CPU cycle.
void cpu_run_cycle(regfile_t *R, memory_t *M, state_t *S) {
  // Fetch and run the next micro instructions for the cycle.
  micro_t *nextMicro = state_next_cycle(S);
  cpu_run_mem(nextMicro, R, M, S);
  cpu_run_data(nextMicro, R, M, S);
  if (nextMicro->incPC) {
    uint16_t PC = (((uint16_t)(R->PCH)) << 8) | R->PCL;
    PC++;
    R->PCL = (uint8_t)PC;
    R->PCH = (uint8_t)(PC >> 8);
  }
  // Poll for interrupts if needed.
  if (cpu_can_poll(nextMicro->data, R, S)) {
    state_set_irq(S);
    state_set_nmi(S);
  }
  return;
}

// Checks if the cpu should poll for interrupts on this cycle.
bool cpu_can_poll(microdata_t dat, regfile_t *R, state_t *S) {
  // We can poll if we are not in the branch fetch, not in an interrupt,
  // and the state is ready to poll.
  return state_can_poll(S) && R->inst != INST_BRK && dat != DAT_BRANCH;
}

// Executes a memory micro instruction.
void cpu_run_mem(micro_t *micro, regfile_t *R, memory_t *M, state_t *S) {
  micromem_t mem = micro->mem;
  switch(mem) {
    case MEM_NOP:
      break;
    case MEM_FETCH:
      if (micro->NMI || micro->IRQ) {
        R->inst = INST_BRK;
      } else {
        R->inst = memory_read(R->PCL, R->PCH, M);
      }
      cpu_fetch_inst(R->inst, micro->NMI, micro->IRQ, S);
      break;
    case MEM_READ_PC_NODEST:
      memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_MDR:
      R->MDR = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_PCH:
      R->PCH = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_ZP_ADDR:
      R->addrH = 0;
      R->addrL = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_ADDRL:
      R->addrL = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_ADDRH:
      R->addrH = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_ZP_PTR:
      R->ptrH = 0;
      R->ptrL = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_PTRL:
      R->ptrL = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_PC_PTRH:
      R->ptrH = memory_read(R->PCL, R->PCH, M);
      break;
    case MEM_READ_ADDR_MDR:
      R->MDR = memory_read(R->addrL, R->addrH, M);
      break;
    case MEM_READ_PTR_MDR:
      R->MDR = memory_read(R->ptrL, R->ptrH, M);
      break;
    case MEM_READ_PTR_ADDRL:
      R->addrL = memory_read(R->ptrL, R->ptrH, M);
      break;
    case MEM_READ_PTR1_ADDRH:
      R->addrH = memory_read(R->ptrL + 1, R->ptrH, M);
      break;
    case MEM_READ_PTR1_PCH:
      R->PCH = memory_read(R->ptrL + 1, R->ptrH, M);
      break;
    case MEM_WRITE_MDR_ADDR:
      memory_write(R->MDR, R->addrL, R->addrH, M);
      break;
    case MEM_WRITE_A_ADDR:
      memory_write(R->A, R->addrL, R->addrH, M);
      break;
    case MEM_WRITE_X_ADDR:
      memory_write(R->X, R->addrL, R->addrH, M);
      break;
    case MEM_WRITE_Y_ADDR:
      memory_write(R->Y, R->addrL, R->addrH, M);
      break;
    case MEM_PUSH_PCL:
      memory_write(R->PCL, R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PUSH_PCH:
      memory_write(R->PCH, R->S, MEMORY_STACK_HIGH, M);
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
      R->PCL = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_PCH:
      R->PCH = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_A:
      R->A = memory_read(R->S, MEMORY_STACK_HIGH, M);
      break;
    case MEM_PULL_P:
      // Pulling P ignores bits 4 and 5.
      R->P = memory_read(R->S, MEMORY_STACK_HIGH, M) & 0xCF;
      break;
    case MEM_NMI_PCL:
      R->PCL = memory_read(MEMORY_NMI_LOW, MEMORY_NMI_HIGH, M);
      break;
    case MEM_NMI_PCH:
      R->PCH = memory_read(MEMORY_NMI_LOW + 1, MEMORY_NMI_HIGH, M);
      break;
    case MEM_RESET_PCL:
      R->PCL = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH, M);
      break;
    case MEM_RESET_PCH:
      R->PCH = memory_read(MEMORY_RESET_LOW + 1, MEMORY_RESET_HIGH, M);
      break;
    case MEM_IRQ_PCL:
      R->PCL = memory_read(MEMORY_IRQ_LOW, MEMORY_IRQ_HIGH, M);
      break;
    case MEM_IRQ_PCH:
      R->PCH = memory_read(MEMORY_IRQ_LOW + 1, MEMORY_IRQ_HIGH, M);
      break;
  }
  return;
}

// Executes a data micro instruction.
void cpu_run_data(micro_t *micro, regfile_t *R, memory_t *M, state_t *S) {

  microdata_t data = micro->data;

  // Some data operations require temperary storage.
  uint16_t res;
  uint8_t carry;
  uint8_t ovf;

  // Branch declarations.
  uint8_t flag;
  bool cond, NMIInt, IRQInt, taken;

  switch(data) {
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
      R->MDR++;
      R->P = (R->P & 0x7D) | (R->MDR & 0x80) | ((R->MDR == 0) << 1);
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
      R->MDR--;
      R->P = (R->P & 0x7D) | (R->MDR & 0x80) | ((R->MDR == 0) << 1);
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
      R->PCL = R->MDR;
      break;
    case DAT_MOV_MDR_A:
      R->A = R->MDR;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_MOV_MDR_X:
      R->X = R->MDR;
      R->P = (R->P & 0x7D) | (R->X & 0x80) | ((R->X == 0) << 1);
      break;
    case DAT_MOV_MDR_Y:
      R->Y = R->MDR;
      R->P = (R->P & 0x7D) | (R->Y & 0x80) | ((R->Y == 0) << 1);
      break;
    case DAT_CLC:
      R->P = R->P & 0xFE;
      break;
    case DAT_CLD:
      R->P = R->P & 0xF7;
      break;
    case DAT_CLI:
      // If there is an interrupt, then we immediately handle it.
      // As such, clearing I should only happen at the end of the state.
      R->P = R->P & 0xFB;
      // NMI has priority
      if (!(micro->NMI) && IRQ) {
        // We need to undo the previous fetch.
        state_clear(S);
        micro->IRQ = IRQ;
        R->inst = INST_BRK;
        cpu_fetch_inst(R->inst, micro->NMI, micro->IRQ, S);
      }
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
      res = (uint16_t)R->A + (uint16_t)(-R->MDR);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_CMP_MDR_X:
      res = (uint16_t)R->X + (uint16_t)(-R->MDR);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_CMP_MDR_Y:
      res = (uint16_t)R->Y + (uint16_t)(-R->MDR);
      R->P = (R->P & 0x7C) | (res & 0x80) | (res >> 8) | ((res == 0) << 1);
      break;
    case DAT_ASL_MDR:
      // The shifted out bit is stored in the carry flag for shifting ops.
      carry = R->MDR >> 7;
      R->MDR = R->MDR << 1;
      R->P = (R->P & 0x7C) | (R->MDR & 0x80) | ((R->MDR == 0) << 1) | carry;
      break;
    case DAT_ASL_A:
      carry = R->A >> 7;
      R->A = R->A << 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_LSR_MDR:
      carry = R->MDR & 0x01;
      R->MDR = R->MDR >> 1;
      R->P = (R->P & 0x7C) | (R->MDR & 0x80) | ((R->MDR == 0) << 1) | carry;
      break;
    case DAT_LSR_A:
      carry = R->A & 0x01;
      R->A = R->A >> 1;
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_ROL_MDR:
      carry = R->MDR >> 7;
      R->MDR = (R->MDR << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->MDR & 0x80) | ((R->MDR == 0) << 1) | carry;
      break;
    case DAT_ROL_A:
      carry = R->A >> 7;
      R->A = (R->A << 1) | (R->P & 0x01);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_ROR_MDR:
      carry = R->MDR & 0x01;
      R->MDR = (R->MDR >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->MDR & 0x80) | ((R->MDR == 0) << 1) | carry;
      break;
    case DAT_ROR_A:
      carry = R->A & 0x01;
      R->A = (R->A >> 1) | (R->P << 7);
      R->P = (R->P & 0x7C) | (R->A & 0x80) | ((R->A == 0) << 1) | carry;
      break;
    case DAT_EOR_MDR_A:
      R->A = R->A ^ R->MDR;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_AND_MDR_A:
      R->A = R->A & R->MDR;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_ORA_MDR_A:
      R->A = R->A | R->MDR;
      R->P = (R->P & 0x7D) | (R->A & 0x80) | ((R->A == 0) << 1);
      break;
    case DAT_ADC_MDR_A:
      res = (uint16_t)R->A + (uint16_t)R->MDR + (uint16_t)(R->P & 0x01);
      ovf = ((R->A & 0x80) == (R->MDR & 0x80)) && ((R->A & 0x80) != (res & 0x80));
      R->A = (uint8_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
           | (ovf << 6) | (res >> 8);
      break;
    case DAT_SBC_MDR_A:
      res = (uint16_t)R->A + (uint16_t)(-R->MDR) + (uint16_t)(-(R->P & 0x01));
      ovf = ((R->A & 0x80) == (R->MDR & 0x80)) && ((R->A & 0x80) != (res & 0x80));
      R->A = (uint8_t)res;
      R->P = (R->P & 0x3C) | (R->A & 0x80) | ((R->A == 0) << 1)
           | (ovf << 6) | (res >> 8);
      break;
    case DAT_BIT_MDR_A:
      R->P = (R->P & 0x3D) | (R->MDR & 0xC0) | (((R->A & R->MDR) == 0) << 1);
      break;
    case DAT_ADD_ADDRL_X:
      res = (uint16_t)R->addrL + (uint16_t)R->X;
      R->addrL = (uint8_t)res;
      R->carry = res >> 8;
      break;
    case DAT_ADD_ADDRL_Y:
      res = (uint16_t)R->addrL + (uint16_t)R->X;
      R->addrL = (uint8_t)res;
      R->carry = res >> 8;
      break;
    case DAT_ADD_PTRL_X:
      R->ptrL = R->ptrL + R->X;
      break;
    case DAT_FIXA_ADDRH:
      if (R->carry) {
        R->addrH += R->carry;
        state_push_cycle(micro->mem, DAT_NOP, false, S);
      }
      R->carry = 0;
      break;
    case DAT_FIX_ADDRH:
      R->addrH = R->addrH + R->carry;
      break;
    case DAT_BRANCH:
      flag = R->inst >> 6;
      cond = (bool)((R->inst >> 5) & 1);
      // Black magic that pulls the proper flag from the status reg.
      flag = (flag >> 1) ? ((R->P >> (flag - 2)) & 1)
                         : ((R->P >> (7 - flag)) & 1);
      taken = (((bool)flag) == cond);
      NMIInt = NMI || micro->NMI;
      IRQInt = IRQ || micro->IRQ;
      res = (uint16_t)R->PCL + (uint16_t)R->MDR;
      R->carry = (uint8_t)(res >> 8);
      if (!taken) {
        // Branches are a special case in the micro op abstraction, as they
        // violate the memory/data difference. I could probably fix it with a
        // different implementation, but theres no real point.
        if (!NMIInt && !IRQInt) {
          uint16_t PC = (((uint16_t)(R->PCH)) << 8) | R->PCL;
          PC++;
          R->PCL = (uint8_t)PC;
          R->PCH = (uint8_t)(PC >> 8);
        }
        R->inst = memory_read(R->PCL, R->PCH, M);
        cpu_fetch_inst(R->inst, NMIInt, IRQInt, S);
      } else if (R->carry) {
        R->PCL = (uint8_t)res;
        state_add_cycle(MEM_NOP, DAT_FIX_ADDRH, false, S);
        state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      } else {
        R->PCL = (uint8_t)res;
        state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      }
      break;
  }
  return;
}

// Decode the instruction and queues the necessary micro instructions.
void cpu_fetch_inst(uint8_t inst, bool NMIInt, bool IRQInt, state_t *S) {
  // Fetch only decodes the instruction if there are no interrupts.
  if (NMIInt) {
    //TODO: Add micro ops.
    return;
  } else if (IRQInt) {
    //TODO: Add micro ops.
    return;
  }
  // No interrupt, so we procede as normal and add the proper micro ops.
  switch (inst) {
    case INST_ORA_IZPX:
      break;
    case INST_ORA_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ORA_MDR_A, true, S);
      break;
    case INST_ORA_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_ORA_MDR_A, true, S);
      break;
    case INST_ORA_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ORA_MDR_A, true, S);
      break;
    case INST_ORA_IZP_Y:
      break;
    case INST_ORA_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ORA_MDR_A, true, S);
      break;
    case INST_ORA_ABY:
      break;
    case INST_ORA_ABX:
      break;
    case INST_AND_IZPX:
      break;
    case INST_AND_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_AND_MDR_A, true, S);
      break;
    case INST_AND_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_AND_MDR_A, true, S);
      break;
    case INST_AND_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_AND_MDR_A, true, S);
      break;
    case INST_AND_IZP_Y:
      break;
    case INST_AND_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_AND_MDR_A, true, S);
      break;
    case INST_AND_ABY:
      break;
    case INST_AND_ABX:
      break;
    case INST_EOR_IZPX:
      break;
    case INST_EOR_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_EOR_MDR_A, true, S);
      break;
    case INST_EOR_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_EOR_MDR_A, true, S);
      break;
    case INST_EOR_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_EOR_MDR_A, true, S);
      break;
    case INST_EOR_IZP_Y:
      break;
    case INST_EOR_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_EOR_MDR_A, true, S);
      break;
    case INST_EOR_ABY:
      break;
    case INST_EOR_ABX:
      break;
    case INST_ADC_IZPX:
      break;
    case INST_ADC_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ADC_MDR_A, true, S);
      break;
    case INST_ADC_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_ADC_MDR_A, true, S);
      break;
    case INST_ADC_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ADC_MDR_A, true, S);
      break;
    case INST_ADC_IZP_Y:
      break;
    case INST_ADC_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ADC_MDR_A, true, S);
      break;
    case INST_ADC_ABY:
      break;
    case INST_ADC_ABX:
      break;
    case INST_STA_IZPX:
      break;
    case INST_STA_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_A_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STA_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_A_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STA_IZP_Y:
      break;
    case INST_STA_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_WRITE_A_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STA_ABY:
      break;
    case INST_STA_ABX:
      break;
    case INST_LDA_IZPX:
      break;
    case INST_LDA_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_A, true, S);
      break;
    case INST_LDA_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_A, true, S);
      break;
    case INST_LDA_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_A, true, S);
      break;
    case INST_LDA_IZP_Y:
      break;
    case INST_LDA_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_A, true, S);
      break;
    case INST_LDA_ABY:
      break;
    case INST_LDA_ABX:
      break;
    case INST_CMP_IZPX:
      break;
    case INST_CMP_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_A, true, S);
      break;
    case INST_CMP_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_A, true, S);
      break;
    case INST_CMP_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_A, true, S);
      break;
    case INST_CMP_IZP_Y:
      break;
    case INST_CMP_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_A, true, S);
      break;
    case INST_CMP_ABY:
      break;
    case INST_CMP_ABX:
      break;
    case INST_SBC_IZPX:
      break;
    case INST_SBC_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SBC_MDR_A, true, S);
      break;
    case INST_SBC_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_SBC_MDR_A, true, S);
      break;
    case INST_SBC_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SBC_MDR_A, true, S);
      break;
    case INST_SBC_IZP_Y:
      break;
    case INST_SBC_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SBC_MDR_A, true, S);
      break;
    case INST_SBC_ABY:
      break;
    case INST_SBC_ABX:
      break;
    case INST_ASL_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ASL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ASL_ACC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ASL_A, true, S);
      break;
    case INST_ASL_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ASL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ASL_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ASL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ASL_ABX:
      break;
    case INST_ROL_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROL_ACC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ROL_A, true, S);
      break;
    case INST_ROL_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROL_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROL_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROL_ABX:
      break;
    case INST_LSR_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_LSR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_LSR_ACC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_LSR_A, true, S);
      break;
    case INST_LSR_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_LSR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_LSR_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_LSR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_LSR_ABX:
      break;
    case INST_ROR_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROR_ACC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_ROR_A, true, S);
      break;
    case INST_ROR_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROR_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_ROR_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_ROR_ABX:
      break;
    case INST_STX_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_X_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STX_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_X_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STX_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_Y, false, S);
      state_add_cycle(MEM_WRITE_X_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_LDX_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_X, true, S);
      break;
    case INST_LDX_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_X, true, S);
      break;
    case INST_LDX_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_X, true, S);
      break;
    case INST_LDX_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_Y, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_X, true, S);
      break;
    case INST_LDX_ABX:
      break;
    case INST_DEC_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_DEC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_DEC_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_DEC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_DEC_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_DEC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_DEC_ABX:
      break;
    case INST_INC_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_INC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_INC_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_INC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_INC_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_INC_MDR, false, S);
      state_add_cycle(MEM_WRITE_MDR_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_INC_ABX:
      break;
    case INST_BIT_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_BIT_MDR_A, true, S);
      break;
    case INST_BIT_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_BIT_MDR_A, true, S);
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
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_Y_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STY_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_WRITE_Y_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_STY_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_WRITE_Y_ADDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_LDY_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_Y, true, S);
      break;
    case INST_LDY_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_Y, true, S);
      break;
    case INST_LDY_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_Y, true, S);
      break;
    case INST_LDY_ZPX:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_ADD_ADDRL_X, false, S);
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_MDR_Y, true, S);
      break;
    case INST_LDY_ABX:
      break;
    case INST_CPY_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_Y, true, S);
      break;
    case INST_CPY_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_Y, true, S);
      break;
    case INST_CPY_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_Y, true, S);
      break;
    case INST_CPX_IMM:
      state_add_cycle(MEM_READ_PC_MDR, DAT_NOP, true, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_X, true, S);
      break;
    case INST_CPX_ZP:
      state_add_cycle(MEM_READ_PC_ZP_ADDR, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_X, true, S);
      break;
    case INST_CPX_ABS:
      state_add_cycle(MEM_READ_PC_ADDRL, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_PC_ADDRH, DAT_NOP, true, S);
      state_add_cycle(MEM_READ_ADDR_MDR, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CMP_MDR_X, true, S);
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
      //TODO: Fix interrupt hijacking.
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, true, S);
      state_add_cycle(MEM_PUSH_P_B, DAT_DEC_S, false, S);
      state_add_cycle(MEM_PUSH_PCL, DAT_DEC_S, false, S);
      state_add_cycle(MEM_PUSH_PCH, DAT_DEC_S, false, S);
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
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_PUSH_P_B, DAT_DEC_S, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_PHA:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_PUSH_A, DAT_DEC_S, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_PLP:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_NOP, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_P, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_PLA:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_NOP, DAT_INC_S, false, S);
      state_add_cycle(MEM_PULL_A, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    case INST_SEC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SEC, true, S);
      break;
    case INST_SEI:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SEI, true, S);
      break;
    case INST_SED:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_SED, true, S);
      break;
    case INST_CLI:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CLI, true, S);
      break;
    case INST_CLC:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CLC, true, S);
      break;
    case INST_CLD:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CLD, true, S);
      break;
    case INST_CLV:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_CLV, true, S);
      break;
    case INST_DEY:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_DEC_Y, true, S);
      break;
    case INST_DEX:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_DEC_X, true, S);
      break;
    case INST_INY:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_INC_Y, true, S);
      break;
    case INST_INX:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_INC_X, true, S);
      break;
    case INST_TAY:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_A_Y, true, S);
      break;
    case INST_TYA:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_Y_A, true, S);
      break;
    case INST_TXA:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_X_A, true, S);
      break;
    case INST_TXS:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_X_S, true, S);
      break;
    case INST_TAX:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_A_X, true, S);
      break;
    case INST_TSX:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_MOV_S_X, true, S);
      break;
    case INST_NOP:
      state_add_cycle(MEM_READ_PC_NODEST, DAT_NOP, false, S);
      state_add_cycle(MEM_FETCH, DAT_NOP, true, S);
      break;
    default:
      printf("Instruction %x is not implemented\n", inst);
      abort();
  }
  return;
}
