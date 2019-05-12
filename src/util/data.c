#include <stdlib.h>
#include "./data.h"

/*
 * Takes in two words and converts them to a double word.
 */
dword_t get_dword(word_t lo, word_t hi) {
  return (((dword_t)hi) << 8) | ((dword_t)lo);
}
