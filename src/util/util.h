#include <stdlib.h>
#include <stdio.h>

// Avoid multiple definitions.
#ifndef _NES_UTIL
#define _NES_UTIL

// Calloc with a NULL check and an out of memory error.
void *xcalloc(size_t nobj, size_t size);

// Malloc with a NULL check and an out of memory error.
void *xmalloc(size_t size);

// Malloc with a NULL check and an out of memory error.
// Randomizes the contents of the results. Used to allocate
// console memory, as some games rely on it being undefined.
void *rand_alloc(size_t size);

// Returns the size of the given file.
size_t get_file_size(FILE *file);

// Prompts the user to open a file, and then opens the file into the
// given pointer.
void open_file(FILE **file);

#endif
