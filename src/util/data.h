#include <stdint.h>

#ifndef _NES_GLOBALS
#define _NES_GLOBALS

typedef uint8_t word_t;
typedef uint16_t dword_t;

// Integer promotion is of the Devil.
#define WORD_MASK 0xFFU

// Converts two words into a double word.
dword_t get_dword(word_t lo, word_t hi);

// Reverses a word.
word_t reverse_word(word_t word);

#endif
