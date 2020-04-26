/*
 * This file and its associated header define the main data sizes used by
 * the NES. In the NES, a word is a byte, and a double word is two bytes.
 * Multiwords are used to aid in the emulation of double word registers
 * that were two word size registers in the original implementation.
 *
 * In addition to these data type definitions, this file provides
 * some basic data utilities. ReverseWord() can be used to bit-wise
 * reverse a word. MsbWord() can be used to isolate the MSB of a given
 * word.
 */

#include "./data.h"

#include <cstdlib>

/*
 * Takes in a word and bit-wise reverses it.
 */
[[gnu::pure]] DataWord ReverseWord(DataWord word) {
  word = ((word & 0x55U) << 1) | ((word & 0xAAU) >> 1);
  word = ((word & 0x33U) << 2) | ((word & 0xCCU) >> 2);
  word = ((word & 0x0FU) << 4) | ((word & 0xF0U) >> 4);
  return word;
}

/*
 * Takes in a word and returns its MSB.
 */
[[gnu::pure]] DataWord MsbWord(DataWord word) {
  // Flip the word, get the LSB, then flip it again.
  DataWord rev = ReverseWord(word);
  return ReverseWord((rev) & (-rev));
}

/*
 * Gets an approximation for the inverse of a floating point number.
 */
[[gnu::pure]] float Inverse(float x) {
#ifdef _NES_HOST_X86
  // If the host is X86, then we can use rcpss for a quick approximation.
  asm("rcpss %1, %0" : "=x" (x) : "x" (x));
  return x;
#else
  /*
   * If our target architecture is not known to be x86, we approximate
   * the result using the "Fast Inverse Square Root" method.
   * Details of this method are available on wikipedia.
   *
   * Since we are looking for the inverse, our magic number is 3/2 that
   * of the original, and our newton-method approximation was calculated
   * using f(y) = (1/y) - x = 0.
   */
  union { float f; uint32_t i; } conv;
  conv.f = x;
  conv.i = 0x7EF4FB9D - conv.i;
  return conv.f * (2.0f - conv.f * x);
#endif
}
