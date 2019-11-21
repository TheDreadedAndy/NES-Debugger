/*
 * This file provides a way to represent the status flags of the 6502 CPU
 * in emulation as a set of boolean values, rather than the register
 * it was in the original implementation. The functions StatusGetVector()
 * and StatusSetVector() can be used to convert between the structural
 * representation and the original register representation as necessary
 * for emulation.
 *
 * The original implementation was layed out as follows:
 * Bit:  7 6 5 4 3 2 1 0
 * flag: N V B B D I Z C
 *       | | | | | | | |
 *       | | | | | | | -> Carry out for unsigned arithmetic.
 *       | | | | | | ---> Zero flag, set when result was zero.
 *       | | | | | -----> IRQ Block flag, prevents IRQ's from being triggered.
 *       | | | | -------> BCD Flag, useless in the NES.
 *       | | -----------> The "B" flag, used to determine where an interrupt
 *       | |              originated.
 *       | -------------> Signed overflow flag.
 *       ---------------> Negative flag, equal to the MSB of the result.
 */

#include "./cpu_status.h"

#include <cstdlib>

#include "../util/data.h"

/*
 * Converts the given status structure with the given value of the B flag
 * into a register representation of CPU status.
 */
DataWord StatusGetVector(CpuStatus *status, bool b_flag) {
  DataWord vector = (status->carry)
                  | (status->zero << 1U)
                  | (status->irq_disable << 2U)
                  | (status->decimal << 3U)
                  | ((b_flag << 4) | 0x20U)
                  | (status->overflow << 6U)
                  | (status->negative << 7U);
  return vector;
}

/*
 * Sets the values in the given status structure using the given register
 * representation of CPU status.
 */
void StatusSetVector(CpuStatus *status, DataWord vector) {
  status->carry = static_cast<bool>(vector & STATUS_FLAG_C);
  status->zero = static_cast<bool>(vector & STATUS_FLAG_Z);
  status->irq_disable = static_cast<bool>(vector & STATUS_FLAG_I);
  status->decimal = static_cast<bool>(vector & STATUS_FLAG_D);
  status->overflow = static_cast<bool>(vector & STATUS_FLAG_V);
  status->negative = static_cast<bool>(vector & STATUS_FLAG_N);
  return;
}
