#include <stdlib.h>
#include "./data.h"

/*
 * Takes in a word and bit-wise reverses it.
 */
word_t reverse_word(word_t word) {
  word = ((word & 0x55U) << 1) | ((word & 0xAAU) >> 1);
  word = ((word & 0x33U) << 2) | ((word & 0xCCU) >> 2);
  word = ((word & 0x0FU) << 4) | ((word & 0xF0U) >> 4);
  return word;
}
