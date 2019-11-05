#ifndef _NES_STATE
#define _NES_STATE

#include <cstdlib>
#include <cstdint>

#include "./cpu.h"

/*
 * Each micro instruction encodes the opperations that the CPU must
 * perform in that cycle.
 *
 * The CPU can perform one data op, one memory op, and a PC increment
 * in a given cycle.
 */
typedef void (Cpu::**CpuOperation)(void);
typedef struct {
  CpuOperation *mem;
  CpuOperation *data;
  bool inc_pc;
} OperationCycle;

/*
 *  System state is managed by a fixed size queue of
 *  micro instructions.
 */
typedef struct {
  OperationCycle *queue;
  size_t front;
  size_t back;
  size_t size;
} StateQueue;

/*
 * A PC operation is simply a boolean value that determines
 * if the PC should be incremented on that cycle or not.
 */
#define PC_NOP false
#define PC_INC true

class CpuState {
  private:
    // Holds the system state, which is represented as a queue/stack
    // of micro ops.
    StateQueue *state_;

    // Holds the last micro operation returned.
    OperationCycle *last_op_;

  public:
    // Inits the state class, creating the state queue.
    CpuState(void);

    // Adds an operation cycle to the state queue.
    void AddCycle(CpuOperation *mem, CpuOperation *data, bool inc_pc);

    // Pushes an operation cycle to the state queue.
    void PushCycle(CpuOperation *mem, CpuOperation *data, bool inc_pc);

    // Dequeues and returns the next operation cycle.
    OperationCycle *NextCycle(void);

    // Returns the last operation cycle dequeued from the state queue.
    OperationCycle *GetLastCycle(void);

    // Clears the state queue.
    void Clear(void);

    // Returns the number of operation cycles in the state queue.
    int GetSize(void);

    // Deletes the given state object.
    ~CpuState();
};

#endif
