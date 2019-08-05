#include <stdlib.h>
#include "./data.h"

/*
 * Takes in two words and converts them to a double word.
 */
dword_t get_dword(word_t lo, word_t hi) {
  return (((dword_t)hi) << 8) | ((dword_t)lo);
}

/*
 * Takes in a word and bit-wise reverses it.
 */
word_t reverse_word(word_t word) {
  word = ((word & 0x55U) << 1) | ((word & 0xAAU) >> 1);
  word = ((word & 0x33U) << 2) | ((word & 0xCCU) >> 2);
  word = ((word & 0x0FU) << 4) | ((word & 0xF0U) >> 4);
  return word;
}
