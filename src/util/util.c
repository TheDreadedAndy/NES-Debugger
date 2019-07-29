#include <stdio.h>
#include <stdlib.h>

/*
 * Attempts to allocate the requested number of bytes using calloc.
 * Aborts on failure.
 */
void *xcalloc(size_t nobj, size_t size) {
  void *res = calloc(nobj, size);

  if (res == NULL) {
    printf("Fatal: Out of Memory\n");
    abort();
  }

  return res;
}

/*
 * Attempts to allocate the requested number of bytes using malloc.
 * Aborts on failure.
 */
void *xmalloc(size_t size) {
  void *res = malloc(size);

  if (res == NULL) {
    printf("Fatal: Out of Memory\n");
    abort();
  }

  return res;
}

/*
 * Gets the file size of the given file.
 * Does not change the current file position.
 */
size_t get_file_size(FILE *file) {
  // Save the current position.
  size_t pos = (size_t) ftell(file);

  // Seek the end and get the file size.
  fseek(file, 0, SEEK_END);
  size_t file_size = (size_t) ftell(file);

  // Reset the position and return.
  fseek(file, pos, SEEK_SET);
  return file_size;
}
