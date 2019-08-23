/*
 * This file contains the memory micro instructions for the 2A03, which
 * are an abstraction that simplfies emulating the cpu's action on each cycle.
 *
 * These functions are not called directly, rather they are added to the state
 * queue by cpu_fetch and then called from the queue each cycle.
 *
 * These functions assume that the memory, registers, and state have been
 * initialized.
 */

#include <stdlib.h>
#include <stdio.h>
#include "./micromem.h"
#include "./microdata.h"
#include "./2A03.h"
#include "./state.h"
#include "./regs.h"
#include "../memory/memory.h"

/*
 * Does nothing.
 */
void mem_nop(void) {
  // I mean, yeah.
  return;
}

/*
 * Fetches the next instruction and adds its cycles to the state queue.
 * Handles interrupts and hijacking.
 *
 * Assumes all the cpu structures have been initialized.
 */
void mem_fetch(void) {
  cpu_fetch(state_last_cycle());
  return;
}

/*
 * Reads a byte from the pc address, then discards it.
 */
void mem_read_pc_nodest(void) {
  memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the mdr.
 */
void mem_read_pc_mdr(void) {
  R->mdr = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pch.
 */
void mem_read_pc_pch(void) {
  R->pc.w[WORD_HI] = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address low register.
 * Zeros out the address high register.
 */
void mem_read_pc_zp_addr(void) {
  R->addr.dw = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address low register.
 */
void mem_read_pc_addrl(void) {
  R->addr.w[WORD_LO] = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the address high register.
 */
void mem_read_pc_addrh(void) {
  R->addr.w[WORD_HI] = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer low register.
 * Zeros out the pointer high register.
 */
void mem_read_pc_zp_ptr(void) {
  R->ptr.dw = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer low register.
 */
void mem_read_pc_ptrl(void) {
  R->ptr.w[WORD_LO] = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the pc address into the pointer high register.
 */
void mem_read_pc_ptrh(void) {
  R->ptr.w[WORD_HI] = memory_read(R->pc.dw);
  return;
}

/*
 * Reads a byte from the addr address into the mdr.
 */
void mem_read_addr_mdr(void) {
  R->mdr = memory_read(R->addr.dw);
  return;
}

/*
 * Reads a byte from the ptr address into the mdr.
 */
void mem_read_ptr_mdr(void) {
  R->mdr = memory_read(R->ptr.dw);
  return;
}

/*
 * Reads a byte from the ptr address into the address low register.
 */
void mem_read_ptr_addrl(void) {
  R->addr.w[WORD_LO] = memory_read(R->ptr.dw);
  return;
}

/*
 * Reads a byte from the ptr address (offset by 1) into the address high
 * register.
 */
void mem_read_ptr1_addrh(void) {
  R->addr.w[WORD_HI] = memory_read(get_dword(R->ptr.w[WORD_LO] + 1U,
                                             R->ptr.w[WORD_HI]));
  return;
}

/*
 * Reads a byte from the ptr address (offset by 1) into the address high
 * register.
 */
void mem_read_ptr1_pch(void) {
  R->pc.w[WORD_HI] = memory_read(get_dword(R->ptr.w[WORD_LO] + 1U,
                                           R->ptr.w[WORD_HI]));
  return;
}

/*
 * Writes the mdr to the addr address.
 */
void mem_write_mdr_addr(void) {
  memory_write(R->mdr, R->addr.dw);
  return;
}

/*
 * Writes A to the addr address.
 */
void mem_write_a_addr(void) {
  memory_write(R->A, R->addr.dw);
  return;
}

/*
 * Writes X to the addr address.
 */
void mem_write_x_addr(void) {
  memory_write(R->X, R->addr.dw);
  return;
}

/*
 * Writes Y to the addr address.
 */
void mem_write_y_addr(void) {
  memory_write(R->Y, R->addr.dw);
  return;
}

/*
 * Writes the pcl to the stack.
 */
void mem_push_pcl(void) {
  memory_write(R->pc.w[WORD_LO], get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Writes the pch to the stack.
 */
void mem_push_pch(void) {
  memory_write(R->pc.w[WORD_HI], get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Writes A to the stack.
 */
void mem_push_a(void) {
  memory_write(R->A, get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Writes the cpu state to the stack. Clears the B flag.
 */
void mem_push_p(void) {
  memory_write((R->P | 0x20U), get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Writes the cpu state to the stack. Sets the B flag.
 */
void mem_push_p_b(void) {
  memory_write((R->P | 0x30U), get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Pushes the state register on the stack with the B flag set, then adds the
 * next cycles of the interrupt according to hijacking behavior.
 */
void mem_brk(void) {
  memory_write((R->P | 0x30U), get_dword(R->S, MEMORY_STACK_HIGH));

  // Allows an nmi to hijack the brk instruction.
  if (nmi_edge) {
    nmi_edge = false;
    irq_ready = false;
    state_add_cycle(&mem_nmi_pcl, &data_sei, PC_NOP);
    state_add_cycle(&mem_nmi_pch, &data_nop, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  } else {
    irq_ready = false;
    state_add_cycle(&mem_irq_pcl, &data_sei, PC_NOP);
    state_add_cycle(&mem_irq_pch, &data_nop, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  }

  return;
}

/*
 * Pushes the state register on the stack with the B flag clear, then adds
 * the next cycles of the interrupt according to hijacking behavior.
 */
void mem_irq(void) {
  memory_write((R->P | 0x20U), get_dword(R->S, MEMORY_STACK_HIGH));

  // Allows an nmi to hijack an irq interrupt.
  if (nmi_edge) {
    nmi_edge = false;
    state_add_cycle(&mem_nmi_pcl, &data_sei, PC_NOP);
    state_add_cycle(&mem_nmi_pch, &data_nop, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  } else {
    state_add_cycle(&mem_irq_pcl, &data_sei, PC_NOP);
    state_add_cycle(&mem_irq_pch, &data_nop, PC_NOP);
    state_add_cycle(&mem_fetch, &data_nop, PC_INC);
  }

  return;
}

/*
 * Pulls the pcl from the stack.
 */
void mem_pull_pcl(void) {
  R->pc.w[WORD_LO] = memory_read(get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Pulls the pch from the stack.
 */
void mem_pull_pch(void) {
  R->pc.w[WORD_HI] = memory_read(get_dword(R->S, MEMORY_STACK_HIGH));
  return;
}

/*
 * Pulls A from the stack. This is the only memory op that sets flags.
 */
void mem_pull_a(void) {
  R->A = memory_read(get_dword(R->S, MEMORY_STACK_HIGH));
  R->P = (R->P & 0x7DU) | (R->A & 0x80U) | ((R->A == 0U) << 1U);
  return;
}

/*
 * Pulls the cpu state from the stack. Zeros out the B flag.
 */
void mem_pull_p(void) {
  R->P = memory_read(get_dword(R->S, MEMORY_STACK_HIGH)) & 0xCFU;
  return;
}

/*
 * Reads from the nmi address into the pcl.
 */
void mem_nmi_pcl(void) {
  R->pc.w[WORD_LO] = memory_read(MEMORY_NMI_ADDR);
  return;
}

/*
 * Reads from the nmi address (offset by 1) into the pch.
 */
void mem_nmi_pch(void) {
  R->pc.w[WORD_HI] = memory_read(MEMORY_NMI_ADDR + 1U);
  return;
}

/*
 * Reads from the reset address into the pcl.
 */
void mem_reset_pcl(void) {
  R->pc.w[WORD_LO] = memory_read(MEMORY_RESET_ADDR);
  return;
}

/*
 * Reads from the reset address (offset by 1) into the pch.
 */
void mem_reset_pch(void) {
  R->pc.w[WORD_HI] = memory_read(MEMORY_RESET_ADDR + 1U);
  return;
}

/*
 * Reads from the irq address into the pcl.
 */
void mem_irq_pcl(void) {
  R->pc.w[WORD_LO] = memory_read(MEMORY_IRQ_ADDR);
  return;
}

/*
 * Reads from the irq address (offset by 1) into the pch.
 */
void mem_irq_pch(void) {
  R->pc.w[WORD_HI] = memory_read(MEMORY_IRQ_ADDR + 1U);
  return;
}
