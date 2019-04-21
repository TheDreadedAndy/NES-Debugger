#include <stdlib.h>

// Avoid multiple definitions.
#ifndef _64_UTIL
#define _64_UTIL

// Calloc with a NULL check and oom error.
void *xcalloc(size_t nobj, size_t size);

// Malloc with a NULL check and oom error.
void *xmalloc(size_t size);

#endif
