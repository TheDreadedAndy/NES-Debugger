/*
 * TODO
 */

#include "./disas.h"

#include <new>
#include <cstdlib>

#include "../util/data.h"
#include "../util/util.h"
#include "../memory/memory.h"

// Stores the information used to disassemble the program.
struct DisasMemory {
  Memory *memory;
  size_t bank;
  DoubleWord pc;
}

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
