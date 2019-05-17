#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "./microops.h"

#ifndef _NES_STATE
#define _NES_STATE

/*
 * The state manages the individual micro instructions that happen
 * on each cycle of each instruction. The largest number of cycles an
 * instruction can take should be 8, but I put 16 to give some breathing
 * room.
 */
#define STATE_MAX_OPS 16

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
  bool nmi;
  bool irq;
} micro_t;

// System state is managed by a fixed size queue of micro instructions
typedef struct state_header {
  micro_t *queue;
  int front;
  int back;
  int size;
} state_t;

// These are the functions that the 2A03 emulation will use to interact with
// the processor state structure.

// Creates a state structure.
state_t *state_new();

// Frees a state structure.
void state_free(state_t *S);

// Adds a micro op to the state queue.
void state_add_cycle(micromem_t mem, microdata_t data, bool inc_pc, state_t *S);

// Pushes a micro op to the state queue.
void state_push_cycle(micromem_t mem, microdata_t data, bool inc_pc, state_t *S);

// Dequeues and returns the next micro op.
micro_t *state_next_cycle(state_t *S);

// Checks if the state is in a position where interrupt polling is allowed.
bool state_can_poll(state_t *S);

// Clears the state queue.
void state_clear(state_t *S);

// Modifies the last item on the queue to flag that there has been an interrupt.
// The last item must be a fetch or branch.
void state_set_irq(state_t *S);
void state_set_nmi(state_t *S);

#endif
