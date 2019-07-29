#include <stdlib.h>
#include <stdio.h>

// Avoid multiple definitions.
#ifndef _NES_UTIL
#define _NES_UTIL

// Calloc with a NULL check and oom error.
void *xcalloc(size_t nobj, size_t size);

// Malloc with a NULL check and oom error.
void *xmalloc(size_t size);

// Returns the size of the given file.
size_t get_file_size(FILE *file);

#endif
