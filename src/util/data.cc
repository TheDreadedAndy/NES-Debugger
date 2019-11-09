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
DataWord ReverseWord(DataWord word) {
  word = ((word & 0x55U) << 1) | ((word & 0xAAU) >> 1);
  word = ((word & 0x33U) << 2) | ((word & 0xCCU) >> 2);
  word = ((word & 0x0FU) << 4) | ((word & 0xF0U) >> 4);
  return word;
}

/*
 * Takes in a word and returns its MSB.
 */
DataWord MsbWord(DataWord word) {
  // Flip the word, get the LSB, then flip it again.
  DataWord rev = ReverseWord(word);
  return ReverseWord((rev) & (-rev));
}
