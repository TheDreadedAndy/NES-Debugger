#ifndef _NES_UTIL
#define _NES_UTIL

#include <cstdlib>
#include <cstdio>

#include "./data.h"

// Returns a randomized array of type word_t. The array is created with new,
// and must be free'd with delete.
DataWord *RandNew(size_t size);

// Returns the size of the given file.
size_t GetFileSize(FILE *file);

// Prompts the user to open a file, and then opens the file into the
// given pointer.
void OpenFile(FILE **file);

// Compares two null-terminated strings and returns true if they are equal.
bool StrEq(const char *str1, const char *str2);

// Copies a null terminated string into a newly allocated string.
char *StrCpy(const char *str);

#endif
