#include <stdint.h>

#ifndef _NES_GLOBALS
#define _NES_GLOBALS

typedef uint8_t DataWord;
typedef uint16_t DoubleWord;
typedef union MultiWord {
  DoubleWord dw;
  DataWord w[2];
} MultiWord;

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
DataWord ReverseWord(DataWord word);

// Gets the MSB of a word.
DataWord MsbWord(DataWord word);

#endif
