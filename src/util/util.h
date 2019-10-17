#include <stdlib.h>
#include <stdio.h>

// Avoid multiple definitions.
#ifndef _NES_UTIL
#define _NES_UTIL

// Returns a randomized array of type word_t. The array is created with new,
// and must be free'd with delete.
word_t *RandNew(size_t size);

// Returns the size of the given file.
size_t GetFileSize(FILE *file);

// Prompts the user to open a file, and then opens the file into the
// given pointer.
void OpenFile(FILE **file);

#endif
