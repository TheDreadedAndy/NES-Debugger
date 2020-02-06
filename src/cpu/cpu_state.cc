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
 *
 * The cpu state queue may never be empty in between cpu cycle emulations,
 * though it may be empty just before a fetch is called.
 */

#include "./cpu_state.h"

#include <new>
#include <cstdlib>
#include <cstdint>

#include "../util/contracts.h"

/*
 * Prepares the state queue for use by reseting its position counters.
 */
CpuState::CpuState(void) {
  Clear();
  return;
}

/* The state class need not clean up any memory on exit. */
CpuState::~CpuState(void) { return; }

/*
 * Adds the given operation to the state queue.
 *
 * Assumes that the state queue is not full.
 */
void CpuState::AddCycle(CpuOperation op) {
  CONTRACT(state_.size < kStateMaxSize_);

  // Adds the given cycle to the queue.
  state_.queue[state_.back] = op;
  state_.back = (state_.back + 1) & kStateMask_;
  state_.size++;

  return;
}

/*
 * Pushes a cycle to the state queue.
 *
 * Assumes that the state queue is not full.
 */
void CpuState::PushCycle(CpuOperation op) {
  CONTRACT(state_.size < kStateMaxSize_);

  // Pushes the operation to the queue.
  state_.front = (state_.front - 1) & kStateMask_;
  state_.queue[state_.front] = op;
  state_.size++;

  return;
}

/*
 * Dequeues the next state cycle and returns it.
 */
CpuOperation CpuState::NextCycle(void) {
  CONTRACT(state_.size > 0);

  // Get the next cycle.
  CpuOperation next_op = state_.queue[state_.front];

  // Remove it from the queue.
  state_.front = (state_.front + 1) & kStateMask_;
  state_.size--;

  return next_op;
}

/*
 * Returns the cycle at the front of the queue.
 */
CpuOperation CpuState::PeekCycle(void) {
  return state_.queue[state_.front];
}

/*
 * Returns the number of elements in the queue of the given state.
 */
int CpuState::GetSize(void) {
  return state_.size;
}

/*
 * Emptys the state queue.
 */
void CpuState::Clear(void) {
  // We need only set the size and index fields to zero, since off-queue
  // operation cycles are undefined.
  state_.front = 0;
  state_.back = 0;
  state_.size = 0;

  return;
}
