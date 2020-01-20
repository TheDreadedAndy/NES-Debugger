/*
 * The functions contained in this file are used by the CPU interpreter
 * to determine when it should stop executing cycles and sync with the
 * APU/PPU. These functions are placed on the state queue along side
 * instructions, and called when needed.
 *
 * Note that none of these instructions check whether or not a vector read
 * is safe. This is a known issue, which will be ignored for now as no
 * implemented memory mapper will have side effects on a vector read.
 */

#include "./cpu.h"

#include <cstdlib>

#include "../memory/memory.h"

/*
 * Checks if a read at the current pc address will have side effects.
 *
 * Assumes the calling CPU has been connected to a valid Memory object.
 */
bool Cpu::CheckPcRead(void) {
  return memory_->CheckRead(regs_->pc.dw);
}

/*
 * Checks if a read at the current addr address will have side effects.
 *
 * Assumes the calling CPU has been connected to a valid Memory object.
 */
bool Cpu::CheckAddrRead(void) {
  return memory_->CheckRead(regs_->addr.dw);
}

/*
 * Checks if a write at the current addr address will have side effects.
 *
 * Assumes the calling CPU has been connected to a valid Memory object.
 */
bool Cpu::CheckAddrWrite(void) {
  return memory_->CheckWrite(regs_->addr.dw);
}

/*
 * Checks if a read at the current pointer address will have side effects.
 *
 * Assumes the calling CPU has been connected to a valid Memory object.
 */
bool Cpu::CheckPtrRead(void) {
  return memory_->CheckRead(regs_->ptr.dw);
}

/*
 * Checks if a write at the current pointer address will have side effects.
 *
 * Assumes the calling CPU has been connected to a valid Memory object.
 */
bool Cpu::CheckPtrWrite(void) {
  return memory_->CheckWrite(regs_->ptr.dw);
}
