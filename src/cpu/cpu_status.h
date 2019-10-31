#ifndef _NES_CPUSTATUS
#define _NES_CPUSTATUS

#include "../util/data.h"

/*
 * This structure is used to track the processor status flags
 * in the CPU emulation.
 */
typedef struct {
  bool carry;
  bool zero;
  bool irq_disable;
  bool decimal;
  bool overflow;
  bool negative;
} CpuStatus;

// Uses the given CPU status structure to create a status bit vector with
// the specified value of B flag.
DataWord StatusCreateVector(CpuStatus *status, bool b_flag);

// Loads the given bit vector into the given CPU status structure.
void StatusLoadVector(CpuStatus *status, DataWord vector);

#endif
