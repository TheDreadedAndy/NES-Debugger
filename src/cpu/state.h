#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "./microops.h"

#ifndef _NES_STATE
#define _NES_STATE

/*
 * Each micro instruction encodes the opperations that the CPU must
 * perform in that cycle.
 *
 * The CPU can perform one data op, one memory op, and a PC increment.
 */
typedef struct micro {
  micromem_t mem;
  microdata_t data;
  bool inc_pc;
} micro_t;

// These are the functions that the 2A03 emulation will use to interact with
// the processor state structure.

// Creates the state structure.
void state_init();

// Frees the state structure.
void state_free();

// Adds a micro op to the state queue.
void state_add_cycle(micromem_t mem, microdata_t data, bool inc_pc);

// Pushes a micro op to the state queue.
void state_push_cycle(micromem_t mem, microdata_t data, bool inc_pc);

// Dequeues and returns the next micro op.
micro_t *state_next_cycle();

// Clears the state queue.
void state_clear();

// Returns the number of operations in the state queue.
int state_get_size();

#endif
