/*
 * TODO
 */

#include "./decode_state.h"

#include <new>
#include <cstdlib>
#include <cstdint>

/*
 * Prepares the state queue for use by reseting its position counters.
 */
DecodeState::DecodeState(void) {
  Clear();
  return;
}

/*
 * Adds the given operation to the micro instruction array.
 *
 * Assumes that the state queue is not full.
 */
void DecodeState::AddCycle(CpuOperation op) {
  micro_[front_] = op;
  front_++;
  return;
}

/*
 * Clears the micro instruction array.
 */
void DecodeState::Clear(void) {
  front_ = 0;
  for (size_t i = 0; i < kStateMaxSize_; i++) { micro_[i] = 0; }
  return;
}

/*
 * Exposes the micro instruction array to the caller.
 */
CpuOperation *DecodeState::Expose(void) {
  return micro_;
}

/* The state class need not clean up any memory on exit. */
DecodeState::~DecodeState(void) { return; }
