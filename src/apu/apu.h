#include <stdlib.h>
#include "../util/data.h"

#ifndef _NES_APU
#define _NES_APU

// Setting this flag enables the high triangle frequencies.
// More accurate, but not pleasant sounding.
extern bool enable_high_freqs;

// Initializes the APU channels.
void apu_init(void);

// Runs an APU cycles, updating the channels.
void apu_run_cycle(void);

// Writes to a memory mapped APU register.
void apu_write(dword_t reg_addr, word_t val);

// Reads from a memory mapped APU register.
word_t apu_read(dword_t reg_addr);

// Frees the APU channel structures.
void apu_free(void);

#endif
