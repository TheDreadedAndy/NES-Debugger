#ifndef _NES_GLOBALS
#define _NES_GLOBALS

#include <cstdint>

typedef uint8_t DataWord;
typedef uint16_t DoubleWord;
typedef union {
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

// Used to convert between 2 words and a double word.
#define GET_DOUBLE_WORD(lo, hi) (static_cast<DoubleWord>(\
                               ((static_cast<DataWord>(hi)) << 8)\
                              | (static_cast<DataWord>(lo))))
#define GET_WORD_HI(dw) (static_cast<DataWord>(\
                        (static_cast<DoubleWord>(dw)) >> 8))
#define GET_WORD_LO(dw) (static_cast<DataWord>(\
                        (static_cast<DoubleWord>(dw)) & 0xFF))

// Reverses a word.
[[gnu::pure]] DataWord ReverseWord(DataWord word);

// Gets the MSB of a word.
[[gnu::pure]] DataWord MsbWord(DataWord word);

#endif
