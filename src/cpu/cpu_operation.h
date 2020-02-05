#ifndef _NES_OP
#define _NES_OP

#include <cstdint>

/*
 * This file describes the encoding scheme used to control the CPU interpreter.
 * Each cycle of the emulation is encoded as a 32-bit operation.
 *
 * The least significant bit represents the value the PC should be
 * incremented by.
 *
 * Bits 1-15 encode the 7-bit data opcode, the 4-bit source register
 * location, and the 4-bit destination register location respectively.
 * These bits are decoded into a data operation to be performed. if
 * the operation is a clear/set on the processor status, the register
 * location bits are instead interpreted as a mask to be used to set/clear
 * flags with.
 *
 * Bits 16-31 encode the 4-bit memory opcode, the 4-bit memory address
 * register location, the 4-bit memory address offset, and the 4-bit
 * register location respectively. If the operation is a store, the offset
 * is instead decoded as another register location to be AND'd with the
 * other register, which is necessary for the implementation of undefined
 * instructions.
 */

// The cpu operation data type, which holds the data detailed above.
typedef uint32_t CpuOperation;

/* PC encoding constants */
#define PC_INC 0x01U

/* Register location enumeration, which can be shifted into a location field */
typedef enum {
  REG_PCL = 0,  REG_PCH = 1,  REG_ADDRL = 2, REG_ADDRH = 3,
  REG_TMP1 = 4, REG_TMP2 = 5, REG_S = 6,     REG_VEC = 8,
  REG_A = 10,   REG_X = 11,   REG_Y = 12,    REG_P = 13,
  REG_INST = 14
} CpuReg;

/* Data opcode enumerations */
typedef enum {
  DAT_NOP = 0,   DAT_INC = 2,  DAT_INCNF = 4,  DAT_DEC = 6,
  DAT_DECNF = 8, DAT_MOV = 10, DAT_MOVNF = 12, DAT_CLS = 14,
  DAT_SET = 16,  DAT_CMP = 18, DAT_ASL = 20,   DAT_LSR = 22,
  DAT_ROL = 24,  DAT_ROR = 26, DAT_XOR = 28,   DAT_OR = 30,
  DAT_AND = 32,  DAT_ADD = 34, DAT_ADC = 36,   DAT_SBC = 38,
  DAT_BIT = 40,  DAT_VFIX = 42
} DataOpcode;

/* Memory opcode enumerations */
typedef enum {
  MEM_NOP = 0,       MEM_READ = 0x10000,   MEM_WRITE = 0x20000,
  MEM_IRQ = 0x30000, MEM_BRQ = 0x40000,    MEM_PHP = 0x50000,
  MEM_PLP = 0x60000, MEM_BRANCH = 0x70000, MEM_READZP = 0x80000
} MemoryOpcode;

/* Offsets for selecting the proper vector to access */
typedef enum {
  OFFSET_NMIL = 0, OFFSET_NMIH = 1, OFFSET_RSTL = 2,
  OFFSET_RSTH = 3, OFFSET_IRQL = 4, OFFSET_IRQH = 5
} MemoryOffset;

/* Macros for setting each field in a cpu operation */
#define DAT_SRC(r)  (((static_cast<int>(r)) <<  8) & 0x00000F00)
#define DAT_DST(r)  (((static_cast<int>(r)) << 12) & 0x0000F000)
#define DAT_MASK(p) (((static_cast<int>(p)) <<  8) & 0x0000FF00)
#define MEM_ADDR(r) (((static_cast<int>(r)) << 20) & 0x00F00000)
#define MEM_OFST(o) (((static_cast<int>(o)) << 24) & 0x0F000000)
#define MEM_OP2(r)  (((static_cast<int>(r)) << 24) & 0x0F000000)
#define MEM_OP1(r)  (((static_cast<int>(r)) << 28) & 0xF0000000)

/* Macros for getting each field in a cpu operation */
#define DATA_OPERATION_MASK 0x0000FFFEU
#define MEMORY_OPERATION_MASK 0xFFFF0000U
#define GET_PC_INC(op) (static_cast<DoubleWord>((static_cast<int>(op)) & 0x01))
#define GET_DAT_OP(op) (static_cast<DataOpcode>((static_cast<int>(op)) & 0xFE))
#define GET_DAT_SRC(op) (static_cast<CpuReg>(((static_cast<int>(op)) >> 8)\
                                                                     & 0x0F))
#define GET_DAT_DST(op) (static_cast<CpuReg>(((static_cast<int>(op)) >> 12)\
                                                                     & 0x0F))
#define GET_DAT_MASK(op) (static_cast<DataWord>(((static_cast<int>(op)) >>8)\
                                                                        & 0xFF))
#define GET_MEM_OP(op) (static_cast<MemoryOpcode>((static_cast<int>(op))\
                                                          & 0x000F0000))
#define GET_MEM_ADDR(op) (static_cast<CpuReg>(((static_cast<int>(op)) >> 20)\
                                                                      & 0x0F))
#define GET_MEM_OFST(op) (static_cast<DoubleWord>(((static_cast<int>(op))\
                                                          >> 24) & 0x0F))
#define GET_MEM_OP2(op) (static_cast<CpuReg>(((static_cast<int>(op)) >> 24)\
                                                                     & 0x0F))
#define GET_MEM_OP1(op) (static_cast<CpuReg>(((static_cast<int>(op)) >> 28)\
                                                                     & 0x0F))

#endif
