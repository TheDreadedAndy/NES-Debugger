#include <stdlib.h>
#include "./data.h"

/*
 * Takes in a word and bit-wise reverses it.
 */
word_t ReverseWord(word_t word) {
  word = ((word & 0x55U) << 1) | ((word & 0xAAU) >> 1);
  word = ((word & 0x33U) << 2) | ((word & 0xCCU) >> 2);
  word = ((word & 0x0FU) << 4) | ((word & 0xF0U) >> 4);
  return word;
}

/*
 * Takes in a word and returns its MSB.
 */
word_t MsbWord(word_t word) {
  // Flip the word, get the LSB, then flip it again.
  word_t rev = ReverseWord(word);
  return ReverseWord((rev) & (-rev));
}
