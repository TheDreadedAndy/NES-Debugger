#ifndef _NES_MICRO_OPS
#define _NES_MICRO_OPS

// Micro instructions are defined as constants as they, strictly speaking,
// do not exist in the 6502. They have been split up into two types of actions
// which can happen in parallel: memory and data.

/* Memory */
#define MEM_NOP 0x00
#define MEM_FETCH 0x01

#define MEM_READ_PC_NODEST 0x02
#define MEM_READ_PC_MDR 0x03
#define MEM_READ_PC_PCH 0x04
#define MEM_READ_PC_ZP_ADDR 0x05
#define MEM_READ_PC_ADDRL 0x06
#define MEM_READ_PC_ADDRH 0x07
#define MEM_READ_PC_ZP_PTR 0x08
#define MEM_READ_PC_PTRL 0x09
#define MEM_READ_PC_PTRH 0x0A
#define MEM_READ_ADDR_MDR 0x0B
#define MEM_READ_PTR_MDR 0x0C
#define MEM_READ_PTR_ADDRL 0x0D
#define MEM_READ_PTR1_ADDRH 0x0E
#define MEM_READ_PTR1_PCH 0x0F

#define MEM_WRITE_MDR_ADDR 0x10
#define MEM_WRITE_A_ADDR 0x11
#define MEM_WRITE_X_ADDR 0x12
#define MEM_WRITE_Y_ADDR 0x13

#define MEM_PUSH_PCL 0x14
#define MEM_PUSH_PCH 0x15
#define MEM_PUSH_A 0x16
#define MEM_PUSH_P 0x17
#define MEM_PUSH_P_B 0x22

#define MEM_PULL_PCL 0x18
#define MEM_PULL_PCH 0x19
#define MEM_PULL_A 0x1A
#define MEM_PULL_P 0x1B

#define MEM_NMI_PCL 0x1C
#define MEM_NMI_PCH 0x1D
#define MEM_RESET_PCL 0x1E
#define MEM_RESET_PCH 0x1F
#define MEM_IRQ_PCL 0x20
#define MEM_IRQ_PCH 0x21

/* Data */
#define DAT_NOP 0x00

#define DAT_INC_S 0x01
#define DAT_INC_X 0x02
#define DAT_INC_Y 0x03
#define DAT_INC_MDR 0x04

#define DAT_DEC_S 0x05
#define DAT_DEC_X 0x06
#define DAT_DEC_Y 0x07
#define DAT_DEC_MDR 0x08

#define DAT_MOV_A_X 0x09
#define DAT_MOV_A_Y 0x0A
#define DAT_MOV_S_X 0x0B
#define DAT_MOV_X_A 0x0C
#define DAT_MOV_X_S 0x0D
#define DAT_MOV_Y_A 0x0E
#define DAT_MOV_MDR_PCL 0x0F
#define DAT_MOV_MDR_A 0x10
#define DAT_MOV_MDR_X 0x11
#define DAT_MOV_MDR_Y 0x12

#define DAT_CLC 0x13
#define DAT_CLD 0x14
#define DAT_CLI 0x15
#define DAT_CLV 0x16

#define DAT_SEC 0x17
#define DAT_SED 0x18
#define DAT_SEI 0x19

#define DAT_CMP_MDR_A 0x1A
#define DAT_CMP_MDR_X 0x1B
#define DAT_CMP_MDR_Y 0x1C

#define DAT_ASL_MDR 0x1D
#define DAT_ASL_A 0x1E
#define DAT_LSR_MDR 0x1F
#define DAT_LSR_A 0x20
#define DAT_ROL_MDR 0x21
#define DAT_ROL_A 0x22
#define DAT_ROR_MDR 0x23
#define DAT_ROR_A 0x24

#define DAT_EOR_MDR_A 0x25
#define DAT_AND_MDR_A 0x26
#define DAT_ORA_MDR_A 0x27
#define DAT_ADC_MDR_A 0x28
#define DAT_SBC_MDR_A 0x29
#define DAT_BIT_MDR_A 0x2A

#define DAT_ADD_ADDRL_X 0x2B
#define DAT_ADD_ADDRL_Y 0x2C
#define DAT_ADD_PTRL_X 0x2D

#define DAT_FIXA_ADDRH 0x2E
#define DAT_FIX_ADDRH 0x2F

#define DAT_BRANCH 0x30

#endif
