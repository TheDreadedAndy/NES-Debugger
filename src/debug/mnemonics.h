#ifndef _NES_MNEMONIC
#define _NES_MNEMONIC

/*
 * This file include the mnemonics for each instruction, and their addressing
 * mode string formats. It is used by the disassembler to generate text.
 */

/* ####################### */
/* # Type 1 instructions # */
/* ####################### */

const char* const kOrMnemonic = "ora";
const char* const kAndMnemonic = "and";
const char* const kXorMnemonic = "eor";
const char* const kAddMnemonic = "adc";
const char* const kStoreAMnemonic = "sta";
const char* const kLoadAMnemonic = "lda";
const char* const kCompareAMnemonic = "cmp";
const char* const kSubtractMnemonic = "sbc";

/* ####################### */
/* # Type 2 instructions # */
/* ####################### */

const char* const kShiftLeftMnemonic = "asl";
const char* const kRotateLeftMnemonic = "rol";
const char* const kShiftRightMnemonic = "lsr";
const char* const kRotateRightMnemonic = "ror";
const char* const kStoreXMnemonic = "stx";
const char* const kLoadXMnemonic = "ldx";
const char* const kDecMnemonic = "dec";
const char* const kIncMnemonic = "inc";

// Type 2 instructions with implied operands.
const char* const kMovXAMnemonic = "txa";
const char* const kMovXSMnemonic = "txs";
const char* const kMovAXMnemonic = "tax";
const char* const kMovSXMnemonic = "tsx";
const char* const kDecXMnemonic = "dex";
const char* const kNopMnemonic = "nop";

/* ####################### */
/* # Type 0 instructions # */
/* ####################### */

const char* const kTestMnemonic = "bit";
const char* const kJumpMnemonic = "jmp";
const char* const kStoreYMnemonic = "sty";
const char* const kLoadYMnemonic = "ldy";
const char* const kCompareYMnemonic = "cpy";
const char* const kCompareXMnemonic = "cpx";

/* ############################# */
/* # Control Flow Instructions # */
/* ############################# */

const char* const kBranchPlusMnemonic = "bpl";
const char* const kBranchMinusMnemonic = "bmi";
const char* const kBranchVClearMnemonic = "bvc";
const char* const kBranchVSetMnemonic = "bvs";
const char* const kBranchCClearMnemonic = "bcc";
const char* const kBranchCSetMnemonic = "bcs";
const char* const kBranchNotEqualMnemonic = "bne";
const char* const kBranchEqualMnemonic = "beq";

const char* const kBreakpointMnemonic = "brk";
const char* const kCallMnemonic = "jsr";
const char* const kReturnInterruptMnemonic = "rti";
const char* const kReturnMnemonic = "rts";

/* ####################### */
/* # Type 8 instructions # */
/* ####################### */

const char* const kPushPMnemonic = "php";
const char* const kClearCMnemonic = "clc";
const char* const kPullPMnemonic = "plp";
const char* const kSetCMnemonic = "sec";
const char* const kPushAMnemonic = "pha";
const char* const kClearIMnemonic = "cli";
const char* const kPullAMnemonic = "pla";
const char* const kSetIMnemonic = "sei";
const char* const kDecYMnemonic = "dey";
const char* const kMovYAMnemonic = "tya";
const char* const kMovAYMnemonic = "tay";
const char* const kClearVMnemonic = "clv";
const char* const kIncYMnemonic = "iny";
const char* const kClearDMnemonic = "cld";
const char* const kIncXMnemonic = "inx";
const char* const kSetDMnemonic = "sed";

/* ########################### */
/* # Addressing mode formats # */
/* ########################### */

const char* const kAddrModeA = " A";
const char* const kAddrModeAbs = " $%02x%02x";
const char* const kAddrModeAbsX = " $%02x%02x,X";
const char* const kAddrModeAbsY = " $%02x%02x,Y";
const char* const kAddrModeImm = " #$%02x";
const char* const kAddrModeInd = " ($%02x%02x)";
const char* const kAddrModeIzpx = " ($%02x,X)";
const char* const kAddrModeIzpY = " ($%02x),Y";
const char* const kAddrModeRel = " $%02x";
const char* const kAddrModeZp = " $%02x";
const char* const kAddrModeZpX = " $%02x,X";
const char* const kAddrModeZpY = " $%02x,Y";

#endif
