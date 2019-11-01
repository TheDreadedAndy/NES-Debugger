/*
 * Whenever a data oppertion is performed, there is a good chance that the
 * cpu status will need to be updated. The cpu status is represented by
 * a register with 7 flags, which are layed out in the following way:
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
 *
 * This file provides a way to convert between the values in the status
 * structure and the original register implementation of CPU status.
 */

#include "./cpu_status.h"

#include <cstdlib>

#include "../util/data.h"

/*
 * Converts the given status structure with the given value of the B flag
 * into a register representation of CPU status.
 */
DataWord StatusGetVector(CpuStatus *status, bool b_flag) {
  DataWord vector = 0U;
  vector = (b_flag) ? vector | 0x30U : vector | 0x20U;
  vector = (status->carry) ? vector | 0x01U : vector;
  vector = (status->zero) ? vector | 0x02U : vector;
  vector = (status->irq_disable) ? vector | 0x04U : vector;
  vector = (status->decimal) ? vector | 0x08U : vector;
  vector = (status->overflow) ? vector | 0x40U : vector;
  reg_Status = (status->negative) ? vector | 0x80U : vector;
  return vector;
}

/*
 * Sets the values in the given status structure using the given register
 * representation of CPU status.
 */
void StatusSetVector(CpuStatus *status, DataWord vector) {
  status->carry = static_cast<bool>(vector & 0x01U);
  status->zero = static_cast<bool>(vector & 0x02U);
  status->irq_disable = static_cast<bool>(vector & 0x04U);
  status->decimal = static_cast<bool>(vector & 0x08U);
  status->overflow = static_cast<bool>(vector & 0x40U);
  status->negative = static_cast<bool>(vector & 0x80U);
  return;
}
