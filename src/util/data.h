#include <stdint.h>

#ifndef _NES_GLOBALS
#define _NES_GLOBALS

typedef uint8_t word_t;
typedef uint16_t dword_t;

// Converts two words into a double word.
dword_t get_dword(word_t lo, word_t hi);

#endif
