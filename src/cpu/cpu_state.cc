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
  state_ = new StateQueue();
  state_->queue = new OperationCycle[STATE_MAX_OPS]();
  last_op_ = new OperationCycle();
  return;
}

/*
 * Uses the given data to create an operation cycle and add it
 * to the state queue.
 */
void CpuState::AddCycle(CpuOperation mem,
                        CpuOperation data, bool inc_pc) {
  // Fill the new operation cycle with the given data.
  OperationCycle *micro = &(state_->queue[state_->back]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;

  // Add the operation cycle to the queue.
  state_->size++;
  CONTRACT(state_->size <= STATE_MAX_OPS);
  state_->back = (state_->back + 1) & STATE_MASK;

  return;
}

/*
 * Pushes a cycle to the state queue.
 */
void CpuState::PushCycle(CpuOperation mem, CpuOperation data, bool inc_pc) {
  // Add the operation cycle to the queue.
  state_->size++;
  CONTRACT(state_->size <= STATE_MAX_OPS);
  state_->front = (state_->front - 1) & STATE_MASK;

  // Fill the new operation cycle with the given data.
  OperationCycle *micro = &(state_->queue[state_->front]);
  micro->mem = mem;
  micro->data = data;
  micro->inc_pc = inc_pc;

  return;
}

/*
 * Dequeues the next state cycle and returns it.
 *
 * The returned operation cycle must not be free'd.
 */
OperationCycle *CpuState::NextCycle(void) {
  CONTRACT(state_->size > 0);

  // Get the next cycle.
  OperationCycle *micro = &(state_->queue[state_->front]);

  // Copy it into the structure which can be accessed by the user.
  last_op_->mem = micro->mem;
  last_op_->data = micro->data;
  last_op_->inc_pc = micro->inc_pc;

  // Remove it from the queue.
  state_->front = (state_->front + 1) & STATE_MASK;
  state_->size--;

  return last_op_;
}

/*
 * Returns the last state cycle dequeued by NextCycle().
 *
 * The returned operation cycle must not be free'd.
 */
OperationCycle *CpuState::GetLastCycle(void) {
  return last_op_;
}

/*
 * Returns the number of elements in the queue of the given state.
 */
int CpuState::GetSize(void) {
  return state_->size;
}

/*
 * Emptys the state queue.
 */
void CpuState::Clear(void) {
  // We need only set the size and index fields to zero, since off-queue
  // operation cycles are undefined.
  state_->front = 0;
  state_->back = 0;
  state_->size = 0;

  return;
}

/*
 * Frees the state queue and last operation.
 */
CpuState::~CpuState() {
  delete[] state_->queue;
  delete state_;
  delete last_op_;
  return;
}
