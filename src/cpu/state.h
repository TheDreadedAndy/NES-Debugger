#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "./microdata.h"
#include "./micromem.h"

#ifndef _NES_STATE
#define _NES_STATE

/*
 * Each micro instruction encodes the opperations that the CPU must
 * perform in that cycle.
 *
 * The CPU can perform one data op, one memory op, and a PC increment.
 */
typedef struct micro {
  micromem_t *mem;
  microdata_t *data;
  bool inc_pc;
} micro_t;

/*
 * A PC operation is simply a boolean value that determines
 * if the PC should be incremented on that cycle or not.
 */
#define PC_NOP false
#define PC_INC true

// These are the functions that the 2A03 emulation will use to interact with
// the processor state structure.

// Creates the state structure.
void state_init(void);

// Frees the state structure.
void state_free(void);

// Adds a micro op to the state queue.
void state_add_cycle(micromem_t *mem, microdata_t *data, bool inc_pc);

// Pushes a micro op to the state queue.
void state_push_cycle(micromem_t *mem, microdata_t *data, bool inc_pc);

// Dequeues and returns the next micro op.
micro_t *state_next_cycle(void);

// Returns the last micro op dequeued by state_next_cycle().
micro_t *state_last_cycle(void);

// Clears the state queue.
void state_clear(void);

// Returns the number of operations in the state queue.
int state_get_size(void);

#endif
