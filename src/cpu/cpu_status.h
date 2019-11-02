#ifndef _NES_CPUSTATUS
#define _NES_CPUSTATUS

#include "../util/data.h"

// These masks represent which bit in the vector contains each flag.
#define STATUS_FLAG_C 0x01U
#define STATUS_FLAG_Z 0x02U
#define STATUS_FLAG_I 0x04U
#define STATUS_FLAG_D 0x08U
#define STATUS_FLAG_V 0x40U
#define STATUS_FLAG_N 0x80U

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
DataWord StatusGetVector(CpuStatus *status, bool b_flag);

// Loads the given bit vector into the given CPU status structure.
void StatusSetVector(CpuStatus *status, DataWord vector);

#endif
