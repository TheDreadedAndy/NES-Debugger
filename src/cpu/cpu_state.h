#ifndef _NES_STATE
#define _NES_STATE

#include <cstdlib>
#include <cstdint>
#include "./cpu_operation.h"


class CpuState {
  private:
    // Used to size the state queue and wrap its position counters.
    static const uint8_t kStateMaxSize_ = 8;
    static const uint8_t kStateMask_ = 0x07;

    /*
     *  System state is managed by a fixed size queue of
     *  micro instructions.
     */
    struct StateQueue {
      CpuOperation queue[kStateMaxSize_];
      uint8_t front;
      uint8_t back;
      uint8_t size;
    };

    // Holds the system state, which is represented as a queue/stack
    // of CPU operations.
    StateQueue state_;

  public:
    // Inits the state class, creating the state queue.
    CpuState(void);

    // Adds an operation cycle to the state queue.
    void AddCycle(CpuOperation op);

    // Pushes an operation cycle to the state queue.
    void PushCycle(CpuOperation op);

    // Dequeues and returns the next operation cycle.
    CpuOperation NextCycle(void);

    // Returns the next operation cycle without dequeueing it.
    CpuOperation PeekCycle(void);

    // Clears the state queue.
    void Clear(void);

    // Returns the number of operation cycles in the state queue.
    int GetSize(void);

    // Deletes the given state object.
    ~CpuState();
};

#endif
