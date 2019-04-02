#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "contracts.h"
#include "util.h"
#include "state.h"
#include "2A03.h"

// Verifys that a state structure is safe to access.
bool is_state(state_t *S) {
  return S != NULL && ((S->start == NULL && S->end == NULL)
           || (S->start != NULL && S->end != NULL));
}

// Allocates and returns a state structure.
state_t *state_new() {
  state_t *S = xcalloc(1, sizeof(state_t));
  CONTRACT(is_state(S));
  return S;
}

// Frees a state structure and all of its contents.
// Requires that the state be well-formed.
void state_free(state_t *S) {
  CONTRACT(is_state(S));

  node_t *temp = S->start;
  // Frees each node and its element.
  while (temp != NULL) {
    S->start = temp;
    temp = temp->next;
    free(S->start->elem);
    free(S->start);
  }
  free(S);

  return;
}

// Checks if the state is empty.
// Requires that the state be well-formed.
bool state_empty(state_t *S) {
  CONTRACT(is_state(S));

  return S->start == NULL && S->end == NULL;
}

// Adds a cycle to the state queue.
// Requires that the state be well-formed.
void state_add_cycle(micromem_t mem, microdata_t data, bool incPC, state_t *S) {
  CONTRACT(is_state(S));

  // Allocate element and load in its data.
  node_t *node = xcalloc(1, sizeof(node_t));
  micro_t *elem = xcalloc(1, sizeof(micro_t));
  elem->mem = mem;
  elem->data = data;
  elem->incPC = incPC;
  node->elem = elem;

  // Update the state.
  if (state_empty(S)) {
    S->start = node;
    S->end = node;
  } else {
    S->end->next = node;
    node->prev = S->end;
    S->end = node;
  }

  return;
}

// Pushes a cycle to the state queue.
// Requires that the state be well-formed.
void state_push_cycle(micromem_t mem, microdata_t data, bool incPC, state_t *S) {
  CONTRACT(is_state(S));

  // Allocate element and load in its data.
  node_t *node = xcalloc(1, sizeof(node_t));
  micro_t *elem = xcalloc(1, sizeof(micro_t));
  elem->mem = mem;
  elem->data = data;
  elem->incPC = incPC;
  node->elem = elem;

  // Update the state.
  if (state_empty(S)) {
    S->start = node;
    S->end = node;
  } else {
    node->next = S->start;
    S->start->prev = node;
    S->start = node;
  }

  return;
}

// Dequeues the next state cycle and returns it.
// Requires that the state be non-empty and well-formed.
micro_t *state_next_cycle(state_t *S) {
  CONTRACT(is_state(S));
  CONTRACT(!state_empty(S));

  // Remove cycle and node, then free the node.
  micro_t *cycle = S->start->elem;
  node_t *temp = S->start;
  S->start = S->start->next;
  if (S->start != NULL) {
    S->start->prev = NULL;
  } else {
    S->end = NULL;
  }
  free(temp);

  return cycle;
}

// Checks if the state is ready to poll for interrupts under normal conditions.
// Requires that the state be valid.
bool state_can_poll(state_t *S) {
  CONTRACT(is_state(S));

  // Checks if there are two micro ops in the state.
  // Polling happens at the end of the second-to-last phase of an inst.
  // Since a fetch should always end a state_t (in general), this is when
  // there are two ops in the queue.
  // See nesdev.com for more on interrupts.
  return S->start != NULL && S->start->next == S->end;
}

// Emptys the state queue, freeing its elements.
// Requires the state to be valid.
void state_clear(state_t *S) {
  CONTRACT(is_state(S));

  node_t *temp = S->start;
  while (temp != NULL) {
    S->start = temp;
    temp = temp->next;
    free(S->start->elem);
    free(S->start);
  }

  S->start = NULL;
  S->end = NULL;

  return;
}

// Sets the IRQ condition to true in the element at the end of the state.
// Assumes that said element will be a fetch call.
// Requires that the state be valid and non-empty.
void state_set_irq(state_t *S) {
  CONTRACT(is_state(S));
  CONTRACT(!state_empty(S));

  S->end->elem->IRQ = IRQ;

  return;
}

// Sets the NMI condition to true in the element at the end of the state.
// Assumes that said element will be a fetch call.
// Requires that the state be valid and non-empty.
void state_set_nmi(state_t *S) {
  CONTRACT(is_state(S));
  CONTRACT(!state_empty(S));

  S->end->elem->NMI = NMI;

  return;
}
