/*
 * These functions manage the state of the 2A03 processor, which is necessary
 * for cycle accurate emulation.
 *
 * The state is implemented as a fixed size queue, with size STATE_MAX_OPS, of
 * micro operation structures. This queue is represented as an array, with the
 * front and back moving depending on the functions the user calls. The front
 * and back can be anywhere, moving about the array in a circle.
 *
 * Each micro op structure contains the information necessary for the processor
 * to execute all the necessary operations of that cycle.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "../util/contracts.h"
#include "../util/util.h"
#include "./state.h"
#include "./2A03.h"

/*
 * The state manages the individual micro instructions that happen
 * on each cycle of each instruction. The largest number of cycles an
 * instruction can take should be 8, but I put 16 to give some breathing
 * room.
 *
 * WARNING: Memory corruption will occur if this is not a power of 2!
 */
#define STATE_MAX_OPS 8

// System state is managed by a fixed size queue of micro instructions
typedef struct state_header {
  micro_t *queue;
  size_t front;
  size_t back;
  size_t size;
} state_t;

/*
 * Global state structure. Unavailable outside this file.
 * Manages the NES system state.
 */
state_t *system_state = NULL;

/*
 * Holds the last micro structure returned. Used in micro op functions.
 * Its contents are obtainable through state_last_cycle()
 */
micro_t *last_micro = NULL;

/*
 * Initializes a state structure of fixed size STATE_MAX_OPS.
 */
void state_init(void) {
  system_state = xcalloc(1, sizeof(state_t));
  system_state->queue = xcalloc(STATE_MAX_OPS, sizeof(micro_t));
  last_micro = xcalloc(1, sizeof(micro_t));
  return;
}

/*
 * Frees the state structure.
 *
 * Assumes the state has been initialized.
 */
void state_free(void) {
  CONTRACT(system_state != NULL);
  free(system_state->queue);
  free(system_state);
  free(last_micro);
  return;
}

/*
 * Checks if the state is empty.
 *
 * Assumes the state has been initialized.
 */
bool state_empty(void) {
  CONTRACT(system_state != NULL);
  return system_state->size == 0;
}

/*
 * Adds a cycle to the state queue.
 *
 * Assumes the state has been initialized.
 */
void state_add_cycle(micromem_t *mem, microdata_t *data, bool inc_pc) {
  CONTRACT(system_state != NULL);
  CONTRACT(system_state->queue != NULL);

  // Fill the new microop with the given data.
  micro_t *micro = &(system_state->queue[system_state->back]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;

  // Add the microop to the queue.
  system_state->size++;
  CONTRACT(system_state->size <= STATE_MAX_OPS);
  system_state->back = (system_state->back + 1) % STATE_MAX_OPS;

  return;
}

/*
 * Pushes a cycle to the state queue.
 *
 * Assumes the state has been initialized.
 */
void state_push_cycle(micromem_t *mem, microdata_t *data, bool inc_pc) {
  CONTRACT(system_state != NULL);
  CONTRACT(system_state->queue != NULL);

  // Add the microop to the queue.
  system_state->size++;
  CONTRACT(system_state->size <= STATE_MAX_OPS);
  system_state->front = (system_state->front + (STATE_MAX_OPS - 1))
                                             % STATE_MAX_OPS;

  // Fill the new microop with the given data.
  micro_t *micro = &(system_state->queue[system_state->front]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;

  return;
}

/*
 * Dequeues the next state cycle and returns it.
 *
 * Assumes the state has been initialized.
 *
 * The returned micro structure must not be free'd.
 */
micro_t *state_next_cycle(void) {
  CONTRACT(system_state != NULL);
  CONTRACT(system_state->queue != NULL);
  CONTRACT(system_state->size > 0);
  CONTRACT(last_micro != NULL);

  // Get the next cycle.
  micro_t *micro = &(system_state->queue[system_state->front]);

  // Copy it into the structure which can be accessed by the user.
  last_micro->mem = micro->mem;
  last_micro->data = micro->data;
  last_micro->inc_pc = micro->inc_pc;

  // Remove it from the queue.
  system_state->front = (system_state->front + 1) % STATE_MAX_OPS;
  system_state->size--;

  return last_micro;
}

/*
 * Returns the last state cycle dequeued by state_next_cycle.
 *
 * Assumes the state has been initialized.
 *
 * The returned micro structure must not be free'd.
 */
micro_t *state_last_cycle(void) {
  CONTRACT(last_micro != NULL);

  return last_micro;
}

/*
 * Returns the number of elements in the queue of the given state.
 *
 * Assumes the state has been initialized.
 */
int state_get_size(void) {
  CONTRACT(system_state != NULL);
  CONTRACT(system_state->size <= STATE_MAX_OPS);
  return system_state->size;
}

/*
 * Emptys the state queue.
 *
 * Assumes the state has been initialized.
 */
void state_clear(void) {
  CONTRACT(system_state != NULL);

  // We need only set the size and index fields to zero, since off-queue
  // micro op structures are undefined.
  system_state->front = 0;
  system_state->back = 0;
  system_state->size = 0;

  return;
}
