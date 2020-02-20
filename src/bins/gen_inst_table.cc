/*
 * TODO
 */

#include <cstdlib>
#include <cstdio>

#include "../cpu/cpu_operation.h"
#include "./decode.h"

// The file which the instruction table will be written to.
const char* const kTableFile = "./bins/inst_table.bin";

// The size of each micro code sequence.
const size_t kMicroCodeSize = 8;

/*
 * Outputs all the results from the decoder into a binary file
 * which can be linked into the program by the compiler.
 */
int main(void) {
  // Get a decoder object.
  Decode *decoder = new Decode();

  // Write all valid instructions to the file.
  FILE *table = fopen(kTableFile, "w");
  CpuOperation *micro_code;
  for (size_t i = 0; i < 258; i++) {
    micro_code = decoder->DecodeInst(i);
    fwrite(micro_code, sizeof(CpuOperation), kMicroCodeSize, table);
  }

  // Close the file and exit.
  fclose(table);
  return 0;
}
