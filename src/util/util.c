#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "./data.h"

/*
 * Attempts to allocate the requested number of bytes using calloc.
 *
 * Aborts on failure.
 */
void *xcalloc(size_t nobj, size_t size) {
  void *res = calloc(nobj, size);
  if (res == NULL) {
    fprintf(stderr, "Fatal: Out of memory\n");
    abort();
  }

  return res;
}

/*
 * Attempts to allocate the requested number of bytes using malloc.
 *
 * Aborts on failure.
 */
void *xmalloc(size_t size) {
  void *res = malloc(size);
  if (res == NULL) {
    fprintf(stderr, "Fatal: Out of memory\n");
    abort();
  }

  return res;
}

/*
 * Attempts to allocate the requested number of bytes using malloc.
 * If the allocation was successful, the contents of the requested
 * memory are randomized.
 *
 * Aborts on failure.
 */
void *rand_alloc(size_t size) {
  // On the first call, rand is seeded with the current system time.
  static bool seeded = false;
  if (!seeded) {
    srand((unsigned int) time(NULL));
    seeded = true;
  }

  // Allocate the bytes and check for errors.
  void *res = malloc(size);
  if (res == NULL) {
    fprintf(stderr, "Fatal: Out of memory\n");
    abort();
  }

  // Randomize the contents of the requested data.
  word_t *bytes = (word_t*) res;
  for (size_t i = 0; i < size; i++) {
    // On some platforms, the high bits of rand are more random than the low
    // bits. As such, the higher bits are shifted down to increase randomness.
    bytes[i] = (word_t) (rand() >> 8);
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
