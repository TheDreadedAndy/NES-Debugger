/*
 * TODO
 */

#include "./decode.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/data.h"
#include "../cpu/machinecode.h"
#include "./decode_state.h"
#include "../cpu/cpu_operation.h"

// Used to access and update the processor status register.
#define P_MASK   0xEFU
#define P_BASE   0x20U
#define P_FLAG_N 0x80U
#define P_FLAG_V 0x40U
#define P_FLAG_B 0x30U
#define P_FLAG_D 0x08U
#define P_FLAG_I 0x04U
#define P_FLAG_Z 0x02U
#define P_FLAG_C 0x01U

/*
 * Creates a new CPU object. The CPU object will not be in a usable state
 * until Connect() and Power() have been called.
 */
Decode::Decode(void) {
  // Prepares the CPU's internal structures.
  state_ = new DecodeState();
  return;
}

/*
 * Decodes the given instruction into cpu micro instructions, then returns
 * the array it created.
 */
CpuOperation *Decode::DecodeInst(DoubleWord inst) {
  // Clear the state.
  state_->Clear();

  /*
   * The 6502 has serveral different addressing modes an instruction can use,
   * and so most instructions will simply call a helper function to decode that
   * addressing mode (with one micro op being changed to perform the desired
   * instruction).
   */
  switch (inst) {
    case INST_ORA_IZPX:
      DecodeIzpx(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ZP:
      DecodeZp(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_IMM:
      DecodeImm(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ABS:
      DecodeAbs(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_IZP_Y:
      DecodeIzpY(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ORA_ZPX:
      DecodeZpR(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_ORA_ABY:
      DecodeAbR(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_ORA_ABX:
      DecodeAbR(DAT_OR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_AND_IZPX:
      DecodeIzpx(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ZP:
      DecodeZp(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_IMM:
      DecodeImm(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ABS:
      DecodeAbs(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_IZP_Y:
      DecodeIzpY(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_AND_ZPX:
      DecodeZpR(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_AND_ABY:
      DecodeAbR(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_AND_ABX:
      DecodeAbR(DAT_AND | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_EOR_IZPX:
      DecodeIzpx(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ZP:
      DecodeZp(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_IMM:
      DecodeImm(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ABS:
      DecodeAbs(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_IZP_Y:
      DecodeIzpY(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_EOR_ZPX:
      DecodeZpR(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_EOR_ABY:
      DecodeAbR(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_EOR_ABX:
      DecodeAbR(DAT_XOR | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_ADC_IZPX:
      DecodeIzpx(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ZP:
      DecodeZp(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_IMM:
      DecodeImm(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ABS:
      DecodeAbs(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_IZP_Y:
      DecodeIzpY(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_ADC_ZPX:
      DecodeZpR(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_ADC_ABY:
      DecodeAbR(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_ADC_ABX:
      DecodeAbR(DAT_ADC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_STA_IZPX:
      DecodeWIzpx(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_IZP_Y:
      DecodeWIzpY(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STA_ZPX:
      DecodeWZpR(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL), REG_X);
      break;
    case INST_STA_ABY:
      DecodeWAbR(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL), REG_Y);
      break;
    case INST_STA_ABX:
      DecodeWAbR(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_ADDRL), REG_X);
      break;
    case INST_LDA_IZPX:
      DecodeIzpx(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_IZP_Y:
      DecodeIzpY(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDA_ZPX:
      DecodeZpR(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_LDA_ABY:
      DecodeAbR(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_LDA_ABX:
      DecodeAbR(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_CMP_IZPX:
      DecodeIzpx(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_IZP_Y:
      DecodeIzpY(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_CMP_ZPX:
      DecodeZpR(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_CMP_ABY:
      DecodeAbR(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_CMP_ABX:
      DecodeAbR(DAT_CMP | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_SBC_IZPX:
      DecodeIzpx(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ZP:
      DecodeZp(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_IMM:
      DecodeImm(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ABS:
      DecodeAbs(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_IZP_Y:
      DecodeIzpY(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SBC_ZPX:
      DecodeZpR(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_SBC_ABY:
      DecodeAbR(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_SBC_ABX:
      DecodeAbR(DAT_SBC | DAT_DST(REG_A) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_ASL_ZP:
      DecodeRwZp(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ACC:
      DecodeNomem(DAT_ASL | DAT_DST(REG_A));
      break;
    case INST_ASL_ABS:
      DecodeRwAbs(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ZPX:
      DecodeRwZpx(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ASL_ABX:
      DecodeRwAbx(DAT_ASL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ZP:
      DecodeRwZp(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ACC:
      DecodeNomem(DAT_ROL | DAT_DST(REG_A));
      break;
    case INST_ROL_ABS:
      DecodeRwAbs(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ZPX:
      DecodeRwZpx(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_ROL_ABX:
      DecodeRwAbx(DAT_ROL | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ZP:
      DecodeRwZp(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ACC:
      DecodeNomem(DAT_LSR | DAT_DST(REG_A));
      break;
    case INST_LSR_ABS:
      DecodeRwAbs(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ZPX:
      DecodeRwZpx(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_LSR_ABX:
      DecodeRwAbx(DAT_LSR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ZP:
      DecodeRwZp(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ACC:
      DecodeNomem(DAT_ROR | DAT_DST(REG_A));
      break;
    case INST_ROR_ABS:
      DecodeRwAbs(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ZPX:
      DecodeRwZpx(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_ROR_ABX:
      DecodeRwAbx(DAT_ROR | DAT_DST(REG_TMP1));
      break;
    case INST_STX_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STX_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STX_ZPY:
      DecodeWZpR(MEM_WRITE | MEM_OP1(REG_X) | MEM_ADDR(REG_ADDRL), REG_Y);
      break;
    case INST_LDX_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDX_ZPY:
      DecodeZpR(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_LDX_ABY:
      DecodeAbR(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_TMP1), REG_Y);
      break;
    case INST_DEC_ZP:
      DecodeRwZp(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ABS:
      DecodeRwAbs(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ZPX:
      DecodeRwZpx(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_DEC_ABX:
      DecodeRwAbx(DAT_DEC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ZP:
      DecodeRwZp(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ABS:
      DecodeRwAbs(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ZPX:
      DecodeRwZpx(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_INC_ABX:
      DecodeRwAbx(DAT_INC | DAT_DST(REG_TMP1));
      break;
    case INST_BIT_ZP:
      DecodeZp(DAT_BIT | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_BIT_ABS:
      DecodeAbs(DAT_BIT | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_JMP:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_PCL) |
                       DAT_MOVNF | DAT_DST(REG_PCL) | DAT_SRC(REG_TMP1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_JMPI:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_TMP1));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_TMP1)
                                                   | MEM_OFST(1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_STY_ZP:
      DecodeWZp(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STY_ABS:
      DecodeWAbs(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL));
      break;
    case INST_STY_ZPX:
      DecodeWZpR(MEM_WRITE | MEM_OP1(REG_Y) | MEM_ADDR(REG_ADDRL), REG_X);
      break;
    case INST_LDY_IMM:
      DecodeImm(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ZP:
      DecodeZp(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ABS:
      DecodeAbs(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_LDY_ZPX:
      DecodeZpR(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_LDY_ABX:
      DecodeAbR(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1), REG_X);
      break;
    case INST_CPY_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPY_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPY_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_Y) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_IMM:
      DecodeImm(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_ZP:
      DecodeZp(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
      break;
    case INST_CPX_ABS:
      DecodeAbs(DAT_CMP | DAT_DST(REG_X) | DAT_SRC(REG_TMP1));
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
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_BRANCH);
      break;
    case INST_BRK:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_BRK | DAT_DECNF | DAT_DST(REG_S));
      break;
    case INST_JSR:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL)
                     | PC_INC);
      state_->AddCycle(MEM_NOP | DAT_NOP);
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_PCL)
                     | DAT_MOVNF | DAT_DST(REG_PCL) | DAT_SRC(REG_TMP1));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_RTI:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
      state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_PLP | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_RTS:
      state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
      state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCL) | MEM_ADDR(REG_S)
                     | DAT_INCNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_OP1(REG_PCH) | MEM_ADDR(REG_S));
      state_->AddCycle(PC_INC);
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case INST_PHP:
      DecodePush(MEM_PHP);
      break;
    case INST_PHA:
      DecodePush(MEM_WRITE | MEM_OP1(REG_A) | MEM_ADDR(REG_S));
      break;
    case INST_PLP:
      DecodePull(MEM_PLP);
      break;
    case INST_PLA:
      DecodePull(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_S)
               | DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_TMP1));
      break;
    case INST_SEC:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_C));
      break;
    case INST_SEI:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_I));
      break;
    case INST_SED:
      DecodeNomem(DAT_SET | DAT_MASK(P_FLAG_D));
      break;
    case INST_CLI:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_I));
      break;
    case INST_CLC:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_C));
      break;
    case INST_CLD:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_D));
      break;
    case INST_CLV:
      DecodeNomem(DAT_CLS | DAT_MASK(P_FLAG_V));
      break;
    case INST_DEY:
      DecodeNomem(DAT_DEC | DAT_DST(REG_Y));
      break;
    case INST_DEX:
      DecodeNomem(DAT_DEC | DAT_DST(REG_X));
      break;
    case INST_INY:
      DecodeNomem(DAT_INC | DAT_DST(REG_Y));
      break;
    case INST_INX:
      DecodeNomem(DAT_INC | DAT_DST(REG_X));
      break;
    case INST_TAY:
      DecodeNomem(DAT_MOV | DAT_DST(REG_Y) | DAT_SRC(REG_A));
      break;
    case INST_TYA:
      DecodeNomem(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_Y));
      break;
    case INST_TXA:
      DecodeNomem(DAT_MOV | DAT_DST(REG_A) | DAT_SRC(REG_X));
      break;
    case INST_TXS:
      DecodeNomem(DAT_MOVNF | DAT_DST(REG_S) | DAT_SRC(REG_X));
      break;
    case INST_TAX:
      DecodeNomem(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_A));
      break;
    case INST_TSX:
      DecodeNomem(DAT_MOV | DAT_DST(REG_X) | DAT_SRC(REG_S));
      break;
    case INST_NOP:
      DecodeNomem(MEM_NOP | DAT_NOP);
      break;
    case EINST_NMI:
      state_->AddCycle(MEM_READ | MEM_ADDR(REG_PCL) | MEM_OP1(REG_TMP2));
      state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCH)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCL)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_P)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCL)
                                                    | MEM_OFST(OFFSET_NMIL)
                     | DAT_SET | DAT_MASK(P_FLAG_I));
      state_->AddCycle(MEM_READ | MEM_ADDR(REG_VEC) | MEM_OP1(REG_PCH)
                                                    | MEM_OFST(OFFSET_NMIH));
      state_->AddCycle(MEM_FETCH | PC_INC);
      break;
    case EINST_IRQ:
      state_->AddCycle(MEM_READ | MEM_ADDR(REG_PCL) | MEM_OP1(REG_TMP2));
      state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCH)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_WRITE | MEM_ADDR(REG_S) | MEM_OP1(REG_PCL)
                     | DAT_DECNF | DAT_DST(REG_S));
      state_->AddCycle(MEM_IRQ | DAT_DECNF | DAT_DST(REG_S));
      break;
    default:
      break;
  }

  return state_->Expose();
}

/*
 * Queues an indexed indirect zero page read instruction using the given
 * micro operation.
 */
void Decode::DecodeIzpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1)
                 | DAT_ADD | DAT_DST(REG_TMP1) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page read instruction using the given micro operation.
 */
void Decode::DecodeZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a read immediate instruction using the given micro operation.
 */
void Decode::DecodeImm(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an absolute addressed read instruction using the given micro
 * operation.
 */
void Decode::DecodeAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an indirect indexed zero page read instruction using the given micro
 * operation.
 */
void Decode::DecodeIzpY(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_VFIX | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page reg indexed read instruction using the given micro
 * operation.
 */
void Decode::DecodeZpR(CpuOperation op, CpuReg reg) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(reg));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an absolute addressed reg indexed read instruction using the given
 * micro operation.
 */
void Decode::DecodeAbR(CpuOperation op, CpuReg reg) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(reg) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_VFIX | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues an implied (no memory access) read instruction using the given
 * micro operation.
 */
void Decode::DecodeNomem(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(MEM_FETCH | op | PC_INC);
  return;
}

/*
 * Queues a zero page read-write instruction using the given micro operation.
 */
void Decode::DecodeRwZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed read-write instruction using the given micro
 * operation.
 */
void Decode::DecodeRwAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a zero page X indexed read-write instruction using the given micro
 * operation.
 */
void Decode::DecodeRwZpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed X indexed read-write instruction using the
 * given micro operation.
 */
void Decode::DecodeRwAbx(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_X) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL) | op);
  state_->AddCycle(MEM_WRITE | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an X indexed indirect zero page write instruction using the given
 * micro operation.
 */
void Decode::DecodeWIzpx(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1)
                 | DAT_ADD | DAT_DST(REG_TMP1) | DAT_SRC(REG_X));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a zero page write instruction using the given micro operation.
 */
void Decode::DecodeWZp(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed write instruction using the given micro
 * operation.
 */
void Decode::DecodeWAbs(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an indirect addressed Y indexed zero page write instruction using the
 * given micro operation.
 */
void Decode::DecodeWIzpY(CpuOperation op) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_TMP1));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_TMP1)
                                                 | MEM_OFST(1)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(REG_Y));
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a reg indexed zero page write instruction using the given micro
 * operation.
 */
void Decode::DecodeWZpR(CpuOperation op, CpuReg reg) {
  state_->AddCycle(MEM_READZP | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL)
                 | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(reg));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues an absolute addressed reg indexed write instruction using the given
 * micro operation.
 */
void Decode::DecodeWAbR(CpuOperation op, CpuReg reg) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRL) | MEM_ADDR(REG_PCL) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_ADDRH) | MEM_ADDR(REG_PCL)
                 | DAT_ADD | DAT_DST(REG_ADDRL) | DAT_SRC(reg) | PC_INC);
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP1) | MEM_ADDR(REG_ADDRL)
                 | DAT_ADD | DAT_DST(REG_ADDRH) | DAT_SRC(REG_TMP2));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a stack push instruction using the given micro operation.
 */
void Decode::DecodePush(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(op | DAT_DECNF | DAT_DST(REG_S));
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Queues a stack pull instruction using the given micro operation.
 */
void Decode::DecodePull(CpuOperation op) {
  state_->AddCycle(MEM_READ | MEM_OP1(REG_TMP2) | MEM_ADDR(REG_PCL));
  state_->AddCycle(DAT_INCNF | DAT_DST(REG_S));
  state_->AddCycle(op);
  state_->AddCycle(MEM_FETCH | PC_INC);
  return;
}

/*
 * Frees the register file, memory, and state.
 */
Decode::~Decode(void) {
  delete state_;
}
