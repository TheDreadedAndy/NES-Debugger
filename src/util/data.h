#include <stdint.h>

#ifndef _NES_GLOBALS
#define _NES_GLOBALS

typedef uint8_t word_t;
typedef uint16_t dword_t;
typedef union multi_word {
  dword_t dw;
  word_t w[2];
} mword_t;

// Determine how the word union should be accessed, based on the byte order
// of the target machine.
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define WORD_LO 0U
#define WORD_HI 1U
#else
#define WORD_LO 1U
#define WORD_HI 0U
#endif

// Integer promotion is of the Devil.
#define WORD_MASK 0xFFU

// Reverses a word.
word_t reverse_word(word_t word);

// Gets the MSB of a word.
word_t msb_word(word_t word);

#endif
