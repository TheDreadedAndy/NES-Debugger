#ifndef _NES_MCHC
#define _NES_MCHC

/*
 * Below are constant definitions for all of the instructions
 * in the 6502 ISA. Undefined instructions are currently unseported.
 *
 * The file is split into different types of instructions. This is a
 * relic of a past implementation, and is irrelevent to the emulator
 * as of this time.
 */

/* ####################### */
/* # Type 1 instructions # */
/* ####################### */

// Opcode/instruction list.
#define OP_ORA         0x00
#define INST_ORA_IZPX  0x01
#define INST_ORA_ZP    0x05
#define INST_ORA_IMM   0x09
#define INST_ORA_ABS   0x0D
#define INST_ORA_IZP_Y 0x11
#define INST_ORA_ZPX   0x15
#define INST_ORA_ABY   0x19
#define INST_ORA_ABX   0x1D

#define OP_AND         0x20
#define INST_AND_IZPX  0x21
#define INST_AND_ZP    0x25
#define INST_AND_IMM   0x29
#define INST_AND_ABS   0x2D
#define INST_AND_IZP_Y 0x31
#define INST_AND_ZPX   0x35
#define INST_AND_ABY   0x39
#define INST_AND_ABX   0x3D

#define OP_EOR         0x40
#define INST_EOR_IZPX  0x41
#define INST_EOR_ZP    0x45
#define INST_EOR_IMM   0x49
#define INST_EOR_ABS   0x4D
#define INST_EOR_IZP_Y 0x51
#define INST_EOR_ZPX   0x55
#define INST_EOR_ABY   0x59
#define INST_EOR_ABX   0x5D

#define OP_ADC         0x60
#define INST_ADC_IZPX  0x61
#define INST_ADC_ZP    0x65
#define INST_ADC_IMM   0x69
#define INST_ADC_ABS   0x6D
#define INST_ADC_IZP_Y 0x71
#define INST_ADC_ZPX   0x75
#define INST_ADC_ABY   0x79
#define INST_ADC_ABX   0x7D

#define OP_STA         0x80
#define INST_STA_IZPX  0x81
#define INST_STA_ZP    0x85
#define INST_STA_ABS   0x8D
#define INST_STA_IZP_Y 0x91
#define INST_STA_ZPX   0x95
#define INST_STA_ABY   0x99
#define INST_STA_ABX   0x9D

#define OP_LDA         0xA0
#define INST_LDA_IZPX  0xA1
#define INST_LDA_ZP    0xA5
#define INST_LDA_IMM   0xA9
#define INST_LDA_ABS   0xAD
#define INST_LDA_IZP_Y 0xB1
#define INST_LDA_ZPX   0xB5
#define INST_LDA_ABY   0xB9
#define INST_LDA_ABX   0xBD

#define OP_CMP         0xC0
#define INST_CMP_IZPX  0xC1
#define INST_CMP_ZP    0xC5
#define INST_CMP_IMM   0xC9
#define INST_CMP_ABS   0xCD
#define INST_CMP_IZP_Y 0xD1
#define INST_CMP_ZPX   0xD5
#define INST_CMP_ABY   0xD9
#define INST_CMP_ABX   0xDD

#define OP_SBC         0xE0
#define INST_SBC_IZPX  0xE1
#define INST_SBC_ZP    0xE5
#define INST_SBC_IMM   0xE9
#define INST_SBC_ABS   0xED
#define INST_SBC_IZP_Y 0xF1
#define INST_SBC_ZPX   0xF5
#define INST_SBC_ABY   0xF9
#define INST_SBC_ABX   0xFD

// Adressing mode list.
#define AM1_IZPX       0x00
#define AM1_ZP         0x04
#define AM1_IMM        0x08
#define AM1_ABS        0x0C
#define AM1_IZP_Y      0x10
#define AM1_ZPX        0x14
#define AM1_ABY        0x18
#define AM1_ABX        0x1C

/* ####################### */
/* # Type 2 instructions # */
/* ####################### */

// Opcode/instruction list.
#define OP_ASL         0x00
#define INST_ASL_ZP    0x06
#define INST_ASL_ACC   0x0A
#define INST_ASL_ABS   0x0E
#define INST_ASL_ZPX   0x16
#define INST_ASL_ABX   0x1E

#define OP_ROL         0x20
#define INST_ROL_ZP    0x26
#define INST_ROL_ACC   0x2A
#define INST_ROL_ABS   0x2E
#define INST_ROL_ZPX   0x36
#define INST_ROL_ABX   0x3E

#define OP_LSR         0x40
#define INST_LSR_ZP    0x46
#define INST_LSR_ACC   0x4A
#define INST_LSR_ABS   0x4E
#define INST_LSR_ZPX   0x56
#define INST_LSR_ABX   0x5E

#define OP_ROR         0x60
#define INST_ROR_ZP    0x66
#define INST_ROR_ACC   0x6A
#define INST_ROR_ABS   0x6E
#define INST_ROR_ZPX   0x76
#define INST_ROR_ABX   0x7E

#define OP_STX         0x80
#define INST_STX_ZP    0x86
#define INST_STX_ABS   0x8E
#define INST_STX_ZPX   0x96

#define OP_LDX         0xA0
#define INST_LDX_IMM   0xA2
#define INST_LDX_ZP    0xA6
#define INST_LDX_ABS   0xAE
#define INST_LDX_ZPX   0xB6
#define INST_LDX_ABX   0xBE

#define OP_DEC         0xC0
#define INST_DEC_ZP    0xC6
#define INST_DEC_ABS   0xCE
#define INST_DEC_ZPX   0xD6
#define INST_DEC_ABX   0xDE

#define OP_INC         0xE0
#define INST_INC_ZP    0xE6
#define INST_INC_ABS   0xEE
#define INST_INC_ZPX   0xF6
#define INST_INC_ABX   0xFE

// Adressing mode list.
#define AM2_IMM        0x00
#define AM2_ZP         0x04
#define AM2_ACC        0x08
#define AM2_ABS        0x0C
#define AM2_ZPX        0x14
#define AM2_ABX        0x1C

// Unformated instructions.
#define INST_TXA       0x8A
#define INST_TXS       0x9A
#define INST_TAX       0xAA
#define INST_TSX       0xBA
#define INST_DEX       0xCA
#define INST_NOP       0xEA

/* ####################### */
/* # Type 0 instructions # */
/* ####################### */

// Opcode/instruction list.
#define OP_BIT         0x20
#define INST_BIT_ZP    0x24
#define INST_BIT_ABS   0x2C

#define OP_JMP         0x40
#define INST_JMP       0x4C

#define OP_JMPI        0x60
#define INST_JMPI      0x6C

#define OP_STY         0x80
#define INST_STY_ZP    0x84
#define INST_STY_ABS   0x8C
#define INST_STY_ZPX   0x94

#define OP_LDY         0xA0
#define INST_LDY_IMM   0xA0
#define INST_LDY_ZP    0xA4
#define INST_LDY_ABS   0xAC
#define INST_LDY_ZPX   0xB4
#define INST_LDY_ABX   0xBC

#define OP_CPY         0xC0
#define INST_CPY_IMM   0xC0
#define INST_CPY_ZP    0xC4
#define INST_CPY_ABS   0xCC

#define OP_CPX         0xE0
#define INST_CPX_IMM   0xE0
#define INST_CPX_ZP    0xE4
#define INST_CPX_ABS   0xEC

// Addressing mode list.
#define AM0_IMM        0x00
#define AM0_ZP         0x04
#define AM0_ABS        0x0C
#define AM0_ZPX        0x14
#define AM0_ABX        0x1C

// These next two catagories are all the instructions where
// the lower nybble is 0 and the msb is not set.

/* ####################### */
/* # Branch instructions # */
/* ####################### */

// Instructions of form xxy10000.
#define INST_BPL       0x10
#define INST_BMI       0x30
#define INST_BVC       0x50
#define INST_BVS       0x70
#define INST_BCC       0x90
#define INST_BCS       0xB0
#define INST_BNE       0xD0
#define INST_BEQ       0xF0

/* ##################################### */
/* # Interrupt/subroutine instructions # */
/* ##################################### */

#define INST_BRK       0x00
#define INST_JSR       0x20
#define INST_RTI       0x40
#define INST_RTS       0x60

/* ####################### */
/* # Type 8 instructions # */
/* ####################### */

#define INST_PHP       0x08
#define INST_CLC       0x18
#define INST_PLP       0x28
#define INST_SEC       0x38
#define INST_PHA       0x48
#define INST_CLI       0x58
#define INST_PLA       0x68
#define INST_SEI       0x78
#define INST_DEY       0x88
#define INST_TYA       0x98
#define INST_TAY       0xA8
#define INST_CLV       0xB8
#define INST_INY       0xC8
#define INST_CLD       0xD8
#define INST_INX       0xE8
#define INST_SED       0xF8

#endif
