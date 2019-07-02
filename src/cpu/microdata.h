#include <stdlib.h>

#ifndef _NES_DATA_OPS
#define _NES_DATA_OPS

// Function type used by the state queue.
typedef void microdata_t(void);

/* Micro Ops */
void data_nop(void);
void data_inc_s(void);
void data_inc_x(void);
void data_inc_y(void);
void data_inc_mdr(void);
void data_dec_s(void);
void data_dec_x(void);
void data_dec_y(void);
void data_dec_mdr(void);
void data_mov_a_x(void);
void data_mov_a_y(void);
void data_mov_s_x(void);
void data_mov_x_a(void);
void data_mov_x_s(void);
void data_mov_y_a(void);
void data_mov_mdr_pcl(void);
void data_mov_mdr_a(void);
void data_mov_mdr_x(void);
void data_mov_mdr_y(void);
void data_clc(void);
void data_cld(void);
void data_cli(void);
void data_clv(void);
void data_sec(void);
void data_sed(void);
void data_sei(void);
void data_cmp_mdr_a(void);
void data_cmp_mdr_x(void);
void data_cmp_mdr_y(void);
void data_asl_mdr(void);
void data_asl_a(void);
void data_lsr_mdr(void);
void data_lsr_a(void);
void data_rol_mdr(void);
void data_rol_a(void);
void data_ror_mdr(void);
void data_ror_a(void);
void data_eor_mdr_a(void);
void data_and_mdr_a(void);
void data_ora_mdr_a(void);
void data_adc_mdr_a(void);
void data_sbc_mdr_a(void);
void data_bit_mdr_a(void);
void data_add_addrl_x(void);
void data_add_addrl_y(void);
void data_add_ptrl_x(void);
void data_fixa_addrh(void);
void data_fix_addrh(void);
void data_fix_pch(void);
void data_branch(void);

#endif
