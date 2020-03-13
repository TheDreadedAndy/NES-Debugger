#ifndef _NES_DISAS
#define _NES_DISAS

#include "../util/data.h"
#include "../memory/memory.h"

// Disassembles the requested number of instructions from the program counter
// on. The disassmbly always uses the requested bank. The returned string
// must be deleted after use.
char *Disassemble(Memory *mem, DoubleWord pc, size_t bank, size_t num_inst);

#endif
