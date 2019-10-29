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

#include "./state.h"

#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "../util/contracts.h"

/*
 * The state manages the individual micro instructions that happen
 * on each cycle of each instruction. The largest number of cycles an
 * instruction can take is 8.
 *
 * WARNING: Memory corruption will occur if this is not a power of 2!
 */
#define STATE_MAX_OPS 8
#define STATE_MASK 0x07U

/*
 * Creates the state queue for the given CpuState with size STATE_MAX_OPS.
 */
CpuState::CpuState(void) {
  state_ = new StateQueue;
  state_->queue = new OperationCycle[STATE_MAX_OPS];
  last_op_ = new OperationCycle;
  return;
}

/*
 * Uses the given data to create an operation cycle and add it
 * to the state queue.
 */
void CpuState::AddCycle(OperationCycle *mem,
                        OperationCycle *data, bool inc_pc) {
  CONTRACT(state_ != NULL);
  CONTRACT(state_->queue != NULL);

  // Fill the new microop with the given data.
  OperationCycle *micro = &(state_->queue[state_->back]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;

  // Add the microop to the queue.
  state_->size++;
  CONTRACT(state_->size <= STATE_MAX_OPS);
  state_->back = (state_->back + 1) & STATE_MASK;

  return;
}

/*
 * Pushes a cycle to the state queue.
 *
 * Assumes the state has been initialized.
 */
void state_push_cycle(micromem_t *mem, microdata_t *data, bool inc_pc) {
  CONTRACT(state_ != NULL);
  CONTRACT(state_->queue != NULL);

  // Add the microop to the queue.
  state_->size++;
  CONTRACT(state_->size <= STATE_MAX_OPS);
  state_->front = (state_->front - 1) & STATE_MASK;

  // Fill the new microop with the given data.
  OperationCycle *micro = &(state_->queue[state_->front]);
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
OperationCycle *state_next_cycle(void) {
  CONTRACT(state_ != NULL);
  CONTRACT(state_->queue != NULL);
  CONTRACT(state_->size > 0);
  CONTRACT(last_micro != NULL);

  // Get the next cycle.
  OperationCycle *micro = &(state_->queue[state_->front]);

  // Copy it into the structure which can be accessed by the user.
  last_micro->mem = micro->mem;
  last_micro->data = micro->data;
  last_micro->inc_pc = micro->inc_pc;

  // Remove it from the queue.
  state_->front = (state_->front + 1) & STATE_MASK;
  state_->size--;

  return last_micro;
}

/*
 * Returns the last state cycle dequeued by state_next_cycle.
 *
 * Assumes the state has been initialized.
 *
 * The returned micro structure must not be free'd.
 */
OperationCycle *state_last_cycle(void) {
  CONTRACT(last_micro != NULL);

  return last_micro;
}

/*
 * Returns the number of elements in the queue of the given state.
 *
 * Assumes the state has been initialized.
 */
int state_get_size(void) {
  CONTRACT(state_ != NULL);
  CONTRACT(state_->size <= STATE_MAX_OPS);
  return state_->size;
}

/*
 * Emptys the state queue.
 *
 * Assumes the state has been initialized.
 */
void state_clear(void) {
  CONTRACT(state_ != NULL);

  // We need only set the size and index fields to zero, since off-queue
  // micro op structures are undefined.
  state_->front = 0;
  state_->back = 0;
  state_->size = 0;

  return;
}

/*
 * Frees the state structure.
 *
 * Assumes the state has been initialized.
 */
void state_free(void) {
  CONTRACT(state_ != NULL);
  free(state_->queue);
  free(state_);
  free(last_micro);
  return;
}
