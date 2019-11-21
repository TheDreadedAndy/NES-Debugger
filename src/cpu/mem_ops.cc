/*
 * This file contains the memory micro instructions for the 2A03, which
 * are an abstraction that simplfies emulating the cpu's action on each cycle.
 * These functions are not called directly (and never should be), rather they
 * are added to the state queue by Cpu::Fetch() and then called from the queue
 * each cycle.
 */

#include "./cpu.h"

#include <cstdlib>
#include <cstdio>

#include "../memory/memory.h"
#include "./cpu_status.h"
#include "./cpu_state.h"

/*
 * Fetches the next instruction and adds its cycles to the state queue.
 * Handles interrupts and hijacking.
 */
void Cpu::MemFetch(void) {
  Fetch(state_->GetLastCycle());
  return;
}

/*
 * Reads a byte from the pc address, then discards it.
 */
void Cpu::MemReadPcNodest(void) {
  memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the mdr.
 */
void Cpu::MemReadPcMdr(void) {
  regs_->mdr = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pch.
 */
void Cpu::MemReadPcPch(void) {
  regs_->pc.w[WORD_HI] = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address low register.
 * Zeros out the address high register.
 */
void Cpu::MemReadPcZpAddr(void) {
  regs_->addr.dw = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address low register.
 */
void Cpu::MemReadPcAddrl(void) {
  regs_->addr.w[WORD_LO] = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address high register.
 */
void Cpu::MemReadPcAddrh(void) {
  regs_->addr.w[WORD_HI] = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer low register.
 * Zeros out the pointer high register.
 */
void Cpu::MemReadPcZpPtr(void) {
  regs_->ptr.dw = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer low register.
 */
void Cpu::MemReadPcPtrl(void) {
  regs_->ptr.w[WORD_LO] = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer high register.
 */
void Cpu::MemReadPcPtrh(void) {
  regs_->ptr.w[WORD_HI] = memory_->Read(regs_->pc.dw);
  return;
}

/*
 * Reads a byte from the addr address into the mdr.
 */
void Cpu::MemReadAddrMdr(void) {
  regs_->mdr = memory_->Read(regs_->addr.dw);
  return;
}

/*
 * Reads a byte from the ptr address into the mdr.
 */
void Cpu::MemReadPtrMdr(void) {
  regs_->mdr = memory_->Read(regs_->ptr.dw);
  return;
}

/*
 * Reads a byte from the ptr address into the address low register.
 */
void Cpu::MemReadPtrAddrl(void) {
  regs_->addr.w[WORD_LO] = memory_->Read(regs_->ptr.dw);
  return;
}

/*
 * Reads a byte from the ptr address (offset by 1) into the address high
 * register.
 */
void Cpu::MemReadPtr1Addrh(void) {
  regs_->ptr.w[WORD_LO]++;
  regs_->addr.w[WORD_HI] = memory_->Read(regs_->ptr.dw);
  regs_->ptr.w[WORD_LO]--;
  return;
}

/*
 * Reads a byte from the ptr address (offset by 1) into the address high
 * register.
 */
void Cpu::MemReadPtr1Pch(void) {
  regs_->ptr.w[WORD_LO]++;
  regs_->pc.w[WORD_HI] = memory_->Read(regs_->ptr.dw);
  regs_->ptr.w[WORD_LO]--;
  return;
}

/*
 * Writes the mdr to the addr address.
 */
void Cpu::MemWriteMdrAddr(void) {
  memory_->Write(regs_->addr.dw, regs_->mdr);
  return;
}

/*
 * Writes A to the addr address.
 */
void Cpu::MemWriteAAddr(void) {
  memory_->Write(regs_->addr.dw, regs_->a);
  return;
}

/*
 * Writes X to the addr address.
 */
void Cpu::MemWriteXAddr(void) {
  memory_->Write(regs_->addr.dw, regs_->x);
  return;
}

/*
 * Writes Y to the addr address.
 */
void Cpu::MemWriteYAddr(void) {
  memory_->Write(regs_->addr.dw, regs_->y);
  return;
}

/*
 * Writes the pcl to the stack.
 */
void Cpu::MemPushPcl(void) {
  memory_->Write(regs_->s.dw, regs_->pc.w[WORD_LO]);
  return;
}

/*
 * Writes the pch to the stack.
 */
void Cpu::MemPushPch(void) {
  memory_->Write(regs_->s.dw, regs_->pc.w[WORD_HI]);
  return;
}

/*
 * Writes A to the stack.
 */
void Cpu::MemPushA(void) {
  memory_->Write(regs_->s.dw, regs_->a);
  return;
}

/*
 * Writes the cpu state to the stack. Clears the B flag.
 */
void Cpu::MemPushP(void) {
  memory_->Write(regs_->s.dw, StatusGetVector(&(regs_->p), false));
  return;
}

/*
 * Writes the cpu state to the stack. Sets the B flag.
 */
void Cpu::MemPushPB(void) {
  memory_->Write(regs_->s.dw, StatusGetVector(&(regs_->p), true));
  return;
}

/*
 * Pushes the state register on the stack with the B flag set, then adds the
 * next cycles of the interrupt according to hijacking behavior.
 */
void Cpu::MemBrk(void) {
  memory_->Write(regs_->s.dw, StatusGetVector(&(regs_->p), true));

  // Allows an nmi to hijack the brk instruction.
  if (nmi_edge_) {
    nmi_edge_ = false;
    irq_ready_ = false;
    state_->AddCycle(&Cpu::MemNmiPcl, &Cpu::DataSei, PC_NOP);
    state_->AddCycle(&Cpu::MemNmiPch, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  } else {
    irq_ready_ = false;
    state_->AddCycle(&Cpu::MemIrqPcl, &Cpu::DataSei, PC_NOP);
    state_->AddCycle(&Cpu::MemIrqPch, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  }

  return;
}

/*
 * Pushes the state register on the stack with the B flag clear, then adds
 * the next cycles of the interrupt according to hijacking behavior.
 */
void Cpu::MemIrq(void) {
  memory_->Write(regs_->s.dw, StatusGetVector(&(regs_->p), false));

  // Allows an nmi to hijack an irq interrupt.
  if (nmi_edge_) {
    nmi_edge_ = false;
    state_->AddCycle(&Cpu::MemNmiPcl, &Cpu::DataSei, PC_NOP);
    state_->AddCycle(&Cpu::MemNmiPch, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  } else {
    state_->AddCycle(&Cpu::MemIrqPcl, &Cpu::DataSei, PC_NOP);
    state_->AddCycle(&Cpu::MemIrqPch, &Cpu::Nop, PC_NOP);
    state_->AddCycle(&Cpu::MemFetch, &Cpu::Nop, PC_INC);
  }

  return;
}

/*
 * Pulls the pcl from the stack.
 */
void Cpu::MemPullPcl(void) {
  regs_->pc.w[WORD_LO] = memory_->Read(regs_->s.dw);
  return;
}

/*
 * Pulls the pch from the stack.
 */
void Cpu::MemPullPch(void) {
  regs_->pc.w[WORD_HI] = memory_->Read(regs_->s.dw);
  return;
}

/*
 * Pulls A from the stack. This is the only memory op that sets flags.
 */
void Cpu::MemPullA(void) {
  regs_->a = memory_->Read(regs_->s.dw);
  regs_->p.negative = regs_->a & STATUS_FLAG_N;
  regs_->p.zero = (regs_->a == 0);
  return;
}

/*
 * Pulls the cpu state from the stack. Zeros out the B flag.
 */
void Cpu::MemPullP(void) {
  StatusSetVector(&(regs_->p), memory_->Read(regs_->s.dw));
  return;
}

/*
 * Reads from the nmi address into the pcl.
 */
void Cpu::MemNmiPcl(void) {
  regs_->pc.w[WORD_LO] = memory_->Read(MEMORY_NMI_ADDR);
  return;
}

/*
 * Reads from the nmi address (offset by 1) into the pch.
 */
void Cpu::MemNmiPch(void) {
  regs_->pc.w[WORD_HI] = memory_->Read(MEMORY_NMI_ADDR + 1U);
  return;
}

/*
 * Reads from the reset address into the pcl.
 */
void Cpu::MemResetPcl(void) {
  regs_->pc.w[WORD_LO] = memory_->Read(MEMORY_RESET_ADDR);
  return;
}

/*
 * Reads from the reset address (offset by 1) into the pch.
 */
void Cpu::MemResetPch(void) {
  regs_->pc.w[WORD_HI] = memory_->Read(MEMORY_RESET_ADDR + 1U);
  return;
}

/*
 * Reads from the irq address into the pcl.
 */
void Cpu::MemIrqPcl(void) {
  regs_->pc.w[WORD_LO] = memory_->Read(MEMORY_IRQ_ADDR);
  return;
}

/*
 * Reads from the irq address (offset by 1) into the pch.
 */
void Cpu::MemIrqPch(void) {
  regs_->pc.w[WORD_HI] = memory_->Read(MEMORY_IRQ_ADDR + 1U);
  return;
}
