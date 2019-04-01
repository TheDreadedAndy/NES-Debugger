#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef _NES_STATE
#define _NES_STATE

// Data type for micro instruction constant.
typedef uint8_t micromem_t;
typedef uint8_t microdata_t;

typedef struct micro {
  micromem_t mem;
  microdata_t data;
  bool incPC;
  bool NMI;
  bool IRQ;
} micro_t;

// System state is managed by a queue of micro instructions
typedef struct queue_header {
  node_t *start;
  node_t *end;
} state_t;

typedef struct node {
  micro_t *elem;
  node_t *next;
  node_t *prev;
} node_t;

// These are the commands that the 2A03 emulation will use to interact with
// the processor state structure.

// Creates a state structure.
state_t *state_new();

// Frees a state structure.
void state_free(state_t *S);

// Adds a micro op to the state queue.
void state_add_cycle(micromem_t mem, microdata_t data, bool incPC, state_t *S);

// Pushes a micro op to the state queue.
void state_push_cycle(micromem_t mem, microdata_t data, bool incPC, state_t *S);

// Dequeues and returns the next micro op.
micro_t *state_next_cycle(state_t *S);

// Checks if the state is in a position where interrupt polling is allowed.
bool state_can_poll(state_t *S);

// Clears the state queue.
void state_clear(state_t *S);

// Modifies the last item on the queue to flag that there has been an interrupt.
// If the last item on the queue is not a fetch request, or if it's empty,
// behavior of the emulator is unknown.
void state_set_irq(state_t *S);
void state_set_nmi(state_t *S);

#endif
