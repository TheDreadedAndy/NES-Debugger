#include <stdio.h>
#include <stdlib.h>

// Calloc with a NULL check.
void *xcalloc(size_t nobj, size_t size) {
  void *res = calloc(nobj, size);

  if (res == NULL) {
    printf("Fatal: Out of Memory\n");
    abort();
  }

  return res;
}

// Malloc with a NULL check.
void *xmalloc(size_t size) {
  void *res = malloc(size);

  if (res == NULL) {
    printf("Fatal: Out of Memory\n");
    abort();
  }

  return res;
}
