/*
 * TODO
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
