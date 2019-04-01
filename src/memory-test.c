#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "util.h"
#include "memory.h"

int main() {
  memory_t *M = memory_new("megaman.nes");
  uint8_t rstLow = memory_read(MEMORY_RESET_LOW, MEMORY_RESET_HIGH, M);
  uint8_t rstHigh = memory_read(MEMORY_RESET_LOW+1, MEMORY_RESET_HIGH, M);
  uint8_t first = memory_read(rstLow, rstHigh, M);
  printf("First known instruction: %x\n", first);
  return 0;
}
