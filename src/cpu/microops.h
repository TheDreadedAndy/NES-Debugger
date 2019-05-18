#include <stdbool.h>

#ifndef _NES_MICRO_OPS
#define _NES_MICRO_OPS

/*
 * Micro instructions are defined as constants as they, strictly speaking,
 * do not exist in the 6502. They have been split up into three types of actions
 * which can happen in parallel: memory, data, and PC.
 */

/* Memory */

/*
 * Memory opperations include fetching, reading, writing,
 * and stack opperations. Reading from an interrupt vector
 * is also classified as a memory opperation.
 */
typedef enum micromem {
  MEM_NOP,
  MEM_FETCH,

  MEM_READ_PC_NODEST,
  MEM_READ_PC_MDR,
  MEM_READ_PC_PCH,
  MEM_READ_PC_ZP_ADDR,
  MEM_READ_PC_ADDRL,
  MEM_READ_PC_ADDRH,
  MEM_READ_PC_ZP_PTR,
  MEM_READ_PC_PTRL,
  MEM_READ_PC_PTRH,
  MEM_READ_ADDR_MDR,
  MEM_READ_PTR_MDR,
  MEM_READ_PTR_ADDRL,
  MEM_READ_PTR1_ADDRH,
  MEM_READ_PTR1_PCH,

  MEM_WRITE_MDR_ADDR,
  MEM_WRITE_A_ADDR,
  MEM_WRITE_X_ADDR,
  MEM_WRITE_Y_ADDR,

  MEM_PUSH_PCL,
  MEM_PUSH_PCH,
  MEM_PUSH_A,
  MEM_PUSH_P,
  MEM_PUSH_P_B,

  MEM_PULL_PCL,
  MEM_PULL_PCH,
  MEM_PULL_A,
  MEM_PULL_P,

  MEM_NMI_PCL,
  MEM_NMI_PCH,
  MEM_RESET_PCL,
  MEM_RESET_PCH,
  MEM_IRQ_PCL,
  MEM_IRQ_PCH,
} micromem_t;

/* Data */

/*
 * Data opperations include any opperation which primarily modifies
 * the register file. This include logical and arithmetic opperations,
 * as well as movement opperations.
 */
typedef enum microdata {
  DAT_NOP,

  DAT_INC_S,
  DAT_INC_X,
  DAT_INC_Y,
  DAT_INC_MDR,

  DAT_DEC_S,
  DAT_DEC_X,
  DAT_DEC_Y,
  DAT_DEC_MDR,

  DAT_MOV_A_X,
  DAT_MOV_A_Y,
  DAT_MOV_S_X,
  DAT_MOV_X_A,
  DAT_MOV_X_S,
  DAT_MOV_Y_A,
  DAT_MOV_MDR_PCL,
  DAT_MOV_MDR_A,
  DAT_MOV_MDR_X,
  DAT_MOV_MDR_Y,

  DAT_CLC,
  DAT_CLD,
  DAT_CLI,
  DAT_CLV,

  DAT_SEC,
  DAT_SED,
  DAT_SEI,

  DAT_CMP_MDR_A,
  DAT_CMP_MDR_X,
  DAT_CMP_MDR_Y,

  DAT_ASL_MDR,
  DAT_ASL_A,
  DAT_LSR_MDR,
  DAT_LSR_A,
  DAT_ROL_MDR,
  DAT_ROL_A,
  DAT_ROR_MDR,
  DAT_ROR_A,

  DAT_EOR_MDR_A,
  DAT_AND_MDR_A,
  DAT_ORA_MDR_A,
  DAT_ADC_MDR_A,
  DAT_SBC_MDR_A,
  DAT_BIT_MDR_A,

  DAT_ADD_ADDRL_X,
  DAT_ADD_ADDRL_Y,
  DAT_ADD_PTRL_X,

  DAT_FIXA_ADDRH,
  DAT_FIX_ADDRH,
  DAT_FIX_PCH,

  DAT_BRANCH,
} microdata_t;

/* PC */

/*
 * A PC operation is simply a boolean value that determines
 * if the PC should be incremented on that cycle or not.
 */
#define PC_NOP false
#define PC_INC true

#endif
