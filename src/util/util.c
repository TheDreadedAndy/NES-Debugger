#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "./data.h"

#ifdef _NES_OSWIN
#include <Windows.h>
#endif

// The size of the buffer used to hold the opened files name in open_file()
#define NAME_BUFFER_SIZE 1024U

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

/*
 * Opens a file open dialogue for the user to select a file, then opens
 * that file. The opened file is placed in the provided pointer.
 */
void open_file(FILE **file) {
  char user_file_name[NAME_BUFFER_SIZE];

#ifdef _NES_OSLIN
  // Get the file name of the file the user opened.
  FILE *user_file = popen("zenity --file-selection", "r");
  fgets(user_file_name, NAME_BUFFER_SIZE, user_file);

  // Remove the newline character added by zenity.
  for (size_t i = 0; i < NAME_BUFFER_SIZE; i++) {
    if (user_file_name[i] == '\n') {
      user_file_name[i] = '\0';
      break;
    }
  }
#endif

#ifdef _NES_OSWIN
  // Prepare the structure which is used to open the file prompt.
  OPENFILENAMEA *prompt_info = xcalloc(1, sizeof(OPENFILENAMEA));
  prompt_info->lStructSize = sizeof(OPENFILENAMEA);
  prompt_info->lpstrFile = &user_file_name;
  prompt_info->nMaxFile = NAME_BUFFER_SIZE;
  memset(&user_file_name, 0, NAME_BUFFER_SIZE);

  // Open the file prompt.
  GetOpenFileNameA(prompt_info);
#endif

  // Open the provided file.
  *file = fopen(user_file_name, "rb");
  return;
}
