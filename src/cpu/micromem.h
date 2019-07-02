#include <stdlib.h>

#ifndef _NES_MEM_OPS
#define _NES_MEM_OPS

// Function type used by the state queue.
typedef void micromem_t(void);

/* Micro Ops */
void mem_nop(void);
void mem_fetch(void);
void mem_read_pc_nodest(void);
void mem_read_pc_mdr(void);
void mem_read_pc_pch(void);
void mem_read_pc_zp_addr(void);
void mem_read_pc_addrl(void);
void mem_read_pc_addrh(void);
void mem_read_pc_zp_ptr(void);
void mem_read_pc_ptrl(void);
void mem_read_pc_ptrh(void);
void mem_read_addr_mdr(void);
void mem_read_ptr_mdr(void);
void mem_read_ptr_addrl(void);
void mem_read_ptr1_addrh(void);
void mem_read_ptr1_pch(void);
void mem_write_mdr_addr(void);
void mem_write_a_addr(void);
void mem_write_x_addr(void);
void mem_write_y_addr(void);
void mem_push_pcl(void);
void mem_push_pch(void);
void mem_push_a(void);
void mem_push_p(void);
void mem_push_p_b(void);
void mem_brk(void);
void mem_irq(void);
void mem_pull_pcl(void);
void mem_pull_pch(void);
void mem_pull_a(void);
void mem_pull_p(void);
void mem_nmi_pcl(void);
void mem_nmi_pch(void);
void mem_reset_pcl(void);
void mem_reset_pch(void);
void mem_irq_pcl(void);
void mem_irq_pch(void);

#endif
