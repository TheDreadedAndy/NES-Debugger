/*
 * TODO
 */

#include "./disas.h"

#include <new>
#include <cstdlib>

#include "../util/data.h"
#include "../util/util.h"
#include "../memory/memory.h"
#include "./mnemonics.h"

// Stores the information used to disassemble the program.
struct DisasMemory {
  Memory *memory;
  size_t bank;
  DoubleWord pc;
};

// Instructions are classified under several different types to aid in
// disassembly.
enum InstType { UNKNOWN, TYPE_0, TYPE_1, TYPE_2, TYPE_8, BRANCH };

/* Helper Functions */
size_t DisassembleInstruction(DisasMemory *ref, char *buf, size_t buf_len);
InstType GetInstructionType(DataWord inst);
size_t DisassembleType8(DataWord inst, char *inst_buf, size_t inst_buf_len);

/*
 * Uses the given memory object, pc, and bank to disassemble the given number
 * of instructions from the location pointed to by the PC onward.
 *
 * Returns a string which must be deleted after use.
 * Assumes the provided memory object is valid.
 */
char *Disassemble(Memory *mem, DoubleWord pc, size_t bank, size_t num_inst) {
  // Store the given information to make tracking it easier.
  DisasMemory ref = { mem, bank, pc };

  // Disassemble the program into a fixed size buffer.
  const size_t buf_max = 256;
  char buf[buf_max];
  size_t buf_size = 0;
  char *disas = NULL;
  char *disas_temp = NULL;
  size_t disas_size = 0;
  size_t inst_size = 0;
  for (size_t i = 0; i < num_inst; i++) {
    inst_size = DisassembleInstruction(&ref, &(buf[buf_size]),
                                       buf_max - buf_size);
    buf_size += inst_size;

    // Since the buffer is too small, we empty it into a string.
    if (!inst_size) {
      // If the instruction could not be disassembled into an empty buffer,
      // we fail and return NULL.
      if (buf_size == 0) {
        if (disas != NULL) { delete[] disas; }
        return NULL;
      }

      // Copy the contents of the buffer to our disassembly string.
      disas_temp = disas;
      disas = StrCat(disas_temp, disas_size, buf, buf_size);
      delete[] disas_temp;
      disas_size += buf_size;

      // Reset the buffer. Dec i to prevent early termination.
      buf_size = 0;
      i--;
    }
  }

  // Check if theres any data in the buffer that needs to be copied over.
  if (buf_size) {
    disas_temp = disas;
    disas = StrCat(disas_temp, disas_size, buf, buf_size);
    delete[] disas_temp;
    disas_size += buf_size;
  }

  return disas;
}

/*
 * Attempts to disassemble the instruction pointed to by the memory reference.
 *
 * If the buffer is too small to hold the disassembled string, 0 is returned
 * and the buffer and refernce are not modified.
 */
size_t DisassembleInstruction(DisasMemory *ref, char *buf, size_t buf_len) {
  // Read the instruction and prepare the buffer.
  const size_t inst_buf_len = 32U;
  char inst_buf[inst_buf_len];
  DataWord inst = ref->memory->Inspect(ref->pc, ref->bank);

  // TODO: Information about the instruction should be printed here.
  // Address, Bank number, Disas offset.

  // Determine the instruction type, and disassemble the instruction.
  size_t mnemonic_len = 0;
  switch(GetInstructionType(inst)) {
    case TYPE_8:
      mnemonic_len = DisassembleType8(inst, inst_buf, inst_buf_len);
      break;
    case TYPE_1:
    case TYPE_2:
    case TYPE_0:
    case BRANCH:
    case UNKNOWN:
    default:
      return 0;
  }

  // TODO: Instruction address mode should be appended here.
  (void)buf;
  (void)buf_len;
  (void)mnemonic_len;

  return 0;
}

/*
 * Determines the type of the given instruction.
 */
InstType GetInstructionType(DataWord inst) {
  if ((inst & 0x3) == 0x1) {
    // Type 1 instructions encode 1 with their low two bits and consist
    // of ALU operations and load/store A.
    return TYPE_1;
  } else if ((inst & 0x3) == 0x2) {
    // Type 2 instructions encode 2 with their low two bits and consist
    // RMW ALU operations and load/store X.
    return TYPE_2;
  } else if ((inst & 0xF) == 0x8) {
    // Type 8 instructions have their low nyble set to 8 and are all
    // implied opperand instructions.
    return TYPE_8;
  } else if ((inst & 0x3) == 0x0) {
    // Type 0 instructions encode 0 with their low two bits and are more
    // varied in opperation.
    return TYPE_0;
  } else if ((inst & 0x1F) == 0x10) {
    // All branch instructions are of the form xxy10000, where xx is the flag
    // to be branched on and y is the value the flag must equal for the branch
    // to be taken.
    return BRANCH;
  } else {
    return UNKNOWN;
  }
}

/*
 * Fills the given buffer with the mnemonic for the given instruction.
 *
 * Assumes the given instruction is type 8.
 */
size_t DisassembleType8(DataWord inst, char *inst_buf, size_t inst_buf_len) {
  const char* const kType8Mnemonics[16] = {
    kPushPMnemonic, kClearCMnemonic, kPullPMnemonic, kSetCMnemonic,
    kPushAMnemonic, kClearIMnemonic, kPullAMnemonic, kSetIMnemonic,
    kDecYMnemonic,  kMovYAMnemonic,  kMovAYMnemonic, kClearVMnemonic,
    kIncYMnemonic,  kClearDMnemonic, kIncXMnemonic,  kSetDMnemonic
  };
  // TODO: Instruction offset must also be changed after disassembly.
  return StrAppend(inst_buf, inst_buf_len, kType8Mnemonics[inst >> 4]);
}
