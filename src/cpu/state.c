/*
 * TODO
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
 * Initializes and returns a state structure of fixed size STATE_MAX_OPS.
 */
state_t *state_new() {
  state_t *S = xcalloc(1, sizeof(state_t));
  S->queue = xcalloc(STATE_MAX_OPS, sizeof(micro_t));
  return S;
}

/*
 * Frees a state structure.
 *
 * Assumes the state is non-null with a non-null queue.
 */
void state_free(state_t *S) {
  CONTRACT(S != NULL);
  free(S->queue);
  free(S);
  return;
}

/*
 * Checks if the state is empty.
 *
 * Requires that the state be non-null and valid.
 */
bool state_empty(state_t *S) {
  CONTRACT(S != NULL);
  return S->size == 0;
}

/*
 * Adds a cycle to the state queue.
 *
 * Requires that the state be non-null and valid.
 */
void state_add_cycle(micromem_t mem, microdata_t data, bool inc_pc, state_t *S) {
  CONTRACT(S != NULL);
  CONTRACT(S->queue != NULL);

  // Fill the new microop with the given data.
  micro_t *micro = &(S->queue[S->back]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;
  micro->nmi = false;
  micro->irq = false;

  // Add the microop to the queue.
  S->size++;
  CONTRACT(S->size <= STATE_MAX_OPS && S->size >= 0);
  S->back = (S->back + 1) % STATE_MAX_OPS;

  return;
}

/*
 * Pushes a cycle to the state queue.
 *
 * Requires that the state be well-formed.
 */
void state_push_cycle(micromem_t mem, microdata_t data, bool inc_pc, state_t *S) {
  CONTRACT(S != NULL);
  CONTRACT(S->queue != NULL);

  // Add the microop to the queue.
  S->size++;
  CONTRACT(S->size <= STATE_MAX_OPS && S->size >= 0);
  S->front = (S->front - 1) % STATE_MAX_OPS;

  // Fill the new microop with the given data.
  micro_t *micro = &(S->queue[S->back]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;
  micro->nmi = false;
  micro->irq = false;

  return;
}

/*
 * Dequeues the next state cycle and returns it.
 *
 * Requires that the state be non-empty and well-formed.
 *
 * The returned micro structure must not be free'd.
 */
micro_t *state_next_cycle(state_t *S) {
  CONTRACT(S != NULL);
  CONTRACT(S->queue != NULL);
  CONTRACT(S->size > 0);

  // Get the next cycle.
  micro_t *micro = &(S->queue[S->front]);

  // Remove it from the queue.
  S->front = (S->front + 1) % STATE_MAX_OPS;
  S->size--;

  return micro;
}

/*
 * Checks if the state is ready to poll for interrupts under normal conditions.
 *
 * Requires that the state be non-null.
 */
bool state_can_poll(state_t *S) {
  CONTRACT(S != NULL);

  /*
   * Checks if there are two micro ops in the state.
   * Polling happens at the end of the second-to-last phase of an inst.
   * Since a fetch should always end a state_t (in general), this is when
   * there are two ops in the queue.
   * See nesdev.com for more on interrupts.
   */
  return S->size == 2;
}

/*
 * Emptys the state queue.
 *
 * Requires the state to be non-null.
 */
void state_clear(state_t *S) {
  CONTRACT(S != NULL);

  // We need only set the size and index fields to zero, since off-queue
  // micro op structures are undefined.
  S->front = 0;
  S->back = 0;
  S->size = 0;

  return;
}

/*
 * Sets the IRQ condition to true in the element at the end of the state.
 *
 * Assumes that said element will be a fetch call.
 *
 * Requires that the state be valid and non-empty.
 */
void state_set_irq(state_t *S) {
  CONTRACT(S != NULL);
  CONTRACT(S->queue != NULL);
  CONTRACT(S->size > 0);

  // Gets the last element in the queue.
  int last_elem = (S->back - 1) % STATE_MAX_OPS;
  micro_t *micro = &(S->queue[last_elem]);

  // Ensures that it is a fetch or branch call.
  CONTRACT(micro->mem == MEM_FETCH || micro->data == DAT_BRANCH);

  // Sets the IRQ condition.
  micro->irq = irq_interrupt;

  return;
}

/*
 * Sets the NMI condition to true in the element at the end of the state.
 *
 * Assumes that said element will be a fetch call.
 *
 * Requires that the state be valid and non-empty.
 */
void state_set_nmi(state_t *S) {
  CONTRACT(S != NULL);
  CONTRACT(S->queue != NULL);
  CONTRACT(S->size > 0);

  // Gets the last element in the queue.
  int last_elem = (S->back - 1) % STATE_MAX_OPS;
  micro_t *micro = &(S->queue[last_elem]);

  // Ensures that it is a fetch or branch call.
  CONTRACT(micro->mem == MEM_FETCH || micro->data == DAT_BRANCH);

  // Sets the NMI condition.
  micro->nmi = nmi_interrupt;

  return;
}
