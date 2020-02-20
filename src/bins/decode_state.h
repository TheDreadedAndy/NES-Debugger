#ifndef _NES_STATE
#define _NES_STATE

#include <cstdlib>
#include <cstdint>
#include "../cpu/cpu_operation.h"

class DecodeState {
  private:
    // Used to size the micro instruction array.
    static const uint8_t kStateMaxSize_ = 8;

    // Holds the micro instructions that have been added.
    CpuOperation micro_[kStateMaxSize_];
    uint8_t front_;

  public:
    // Inits the state class.
    DecodeState(void);

    // Adds an operation cycle to the micro instruction array..
    void AddCycle(CpuOperation op);

    // Clears the micro instruction array..
    void Clear(void);

    // Exposes the micro instruction array to the caller.
    CpuOperation *Expose(void);

    // Deletes the given state object.
    ~DecodeState();
};

#endif
