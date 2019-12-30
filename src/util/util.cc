/*
 * This file provides several memory, file, and string utilities.
 *
 * RandNew() creates an array of DataWords with the requested size, then
 * randomizes the contents of this array. This function is used mainly in
 * memory emulation, as serveral games require memory to be in an undefined
 * on power up.
 *
 * The file utilities provide the emulator with an OS independent way
 * to interact with the file system. These functions include getting the
 * size of a file, opening a file open gui to the user, getting the
 * configuration folder for the emulator, recursively creating a folder
 * and its parents, and joining two paths together with the proper folder
 * seperator.
 *
 * The string utilities provide an easy way to perform basic string operations.
 * Strings can be equated and copied using the functions in this file.
 */

#include "./util.h"

#include <new>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <cerrno>

#ifdef _NES_OSWIN
#include <windows.h>
#endif

#ifdef _NES_OSLIN
#include <sys/stat.h>
#endif

#include "./data.h"

#ifdef _NES_OSLIN
// The maximum valid path size on linux.
// Note that windows.h defines this constant as 260.
#define MAX_PATH 4096U
#endif

/* Helper function declarations */
bool CreateFolder(const char *path);

/*
 * Allocates the requested number of words using new, then randomizes the
 * resulting array.
 */
DataWord *RandNew(size_t size) {
  // On the first call, rand is seeded with the current system time.
  static bool seeded = false;
  if (!seeded) {
    srand(static_cast<unsigned int>(time(NULL)));
    seeded = true;
  }

  // Allocate the requested data.
  DataWord *res = new DataWord[size];

  // Randomize the contents of the requested data.
  for (size_t i = 0; i < size; i++) {
    // On some platforms, the high bits of rand are more random than the low
    // bits. As such, the higher bits are shifted down to increase randomness.
    res[i] = static_cast<DataWord>(rand() >> 8);
  }

  return res;
}

/*
 * Gets the file size of the given file.
 * Does not change the current file position.
 */
size_t GetFileSize(FILE *file) {
  // Save the current position.
  size_t pos = static_cast<size_t>(ftell(file));

  // Seek the end and get the file size.
  fseek(file, 0, SEEK_END);
  size_t file_size = static_cast<size_t>(ftell(file));

  // Reset the position and return.
  fseek(file, pos, SEEK_SET);
  return file_size;
}

/*
 * Opens a file open dialogue for the user to select a file, then opens
 * that file. The opened file is placed in the provided pointer.
 */
void OpenFile(FILE **file) {
  char user_file_name[MAX_PATH + 1];

#ifdef _NES_OSLIN
  // Get the file name of the file the user opened.
  FILE *user_file = popen("zenity --file-selection", "r");
  if (fgets(user_file_name, MAX_PATH, user_file) == NULL) {
    *file = NULL;
    return;
  }

  // Remove the newline character added by zenity.
  for (size_t i = 0; i < MAX_PATH; i++) {
    if (user_file_name[i] == '\n') {
      user_file_name[i] = '\0';
      break;
    }
  }
#endif

#ifdef _NES_OSWIN
  // Prepare the structure which is used to open the file prompt.
  OPENFILENAMEA *prompt_info = new OPENFILENAMEA;
  prompt_info->lStructSize = sizeof(OPENFILENAMEA);
  prompt_info->lpstrFile = user_file_name;
  prompt_info->nMaxFile = MAX_PATH;
  memset(user_file_name, 0, MAX_PATH);

  // Open the file prompt.
  GetOpenFileNameA(prompt_info);
#endif

  // Open the provided file.
  *file = fopen(user_file_name, "rb");
  return;
}

/*
 * Attempts to create all missing folders in the given path.
 *
 * Assumes paths will be written with ascii characters.
 *
 * Returns false on failure.
 * Returns true on success or if the folders already exist.
 */
bool CreatePath(const char *path) {
  // Create a buffer to hold the provided path, and copy the path in.
  char buf[MAX_PATH + 1];
  size_t path_len = strlen(path);
  if (path_len > MAX_PATH) { return false; }
  for (size_t i = 0; i <= path_len; i++) { buf[i] = path[i]; }

  // Attempt to create each folder in the path, starting from the latest
  // and stopping once one is successfully created (or exists).
  size_t sub_path_len = path_len;
  while (!CreateFolder(buf)) {
    // Move the null terminator up a level in the path.
    while (sub_path_len > 0) {
      sub_path_len--;
      if (buf[sub_path_len] == kSlash) {
        buf[sub_path_len] = '\0';
        break;
      }
    }

    // We failed to create the top level folder.
    if (sub_path_len == 0) { return false; }
  }

  // Repeats the process in the reverse order, creating folders from
  // the first one that existed one at a time.
  while (sub_path_len < path_len) {
    sub_path_len++;
    if (buf[sub_path_len] == '\0') {
      buf[sub_path_len] = kSlash;
      // If create folder fails here, then it was for some reason other then
      // the parents not existing and we return false.
      if (!CreateFolder(buf)) { return false; }
    }
  }

  return true;
}

/*
 * Attempts to create the given folder.
 *
 * Returns true on success, or if the given folder exists already.
 * Returns false on failure.
 */
bool CreateFolder(const char *path) {
#if defined(_NES_OSLIN)
  // If the folder is being created on linux, we assume that only the active
  // user should have permission to use it.
  return (mkdir(path, S_IRWXU) == 0) || (errno == EEXIST);

#elif defined(_NES_OSWIN)
  return (CreateDirectoryA(path, NULL))
      || (GetLastError() == ERROR_ALREADY_EXISTS);

#else
  return false;

#endif
}

/*
 * Gets the location of the configuration folder, which depends on the users OS.
 * On Windows, this will be C:/Users/USER/My Documents/ndb
 * On Linux, this will be /home/USER/.config/ndb
 *
 * This function always returns some valid string, though the path may be
 * inaccessible.
 *
 * The returned string must be deleted after use.
 */
char *GetRootFolder(void) {
#if defined(_NES_OSLIN)
  char *home = getenv("HOME");
  if (home == NULL) {
    return StrCpy(kUndefinedRootFolder);
  } else {
    return JoinPaths(home, kLinuxSubPath);
  }

#elif defined(_NES_OSWIN)
  // Attempt to find the users documents folder.
  char path[MAX_PATH + 1];
  memset(path, 0, MAX_PATH);
  HRESULT res = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, -1,
                                SHGFP_TYPE_CURRENT, path);

  // If the users documents folder could not be obtained, we return the
  // undefined root folder.
  if (res != S_OK) {
    fprintf(stderr, "Error: Failed to find the default folder\n");
    return StrCpy(kUndefinedRootFolder);
  } else {
    return JoinPaths(path, kWindowsSubPath);
  }

#else
  return StrCpy(kUndefinedRootFolder);

#endif
}

/*
 * Joins the given paths into one path, adding a kSlash between the paths.
 * If the first path is an empty string, a copy of the second path is
 * returned (so that it can be used as a reletive path).
 *
 * Assumes both paths are non-null and valid strings.
 *
 * The returned string must be deleted after use.
 */
char *JoinPaths(const char *path1, const char *path2) {
  // Return the second path if the first is the undefined root.
  if (StrEq(kUndefinedRootFolder, path1)) { return StrCpy(path2); }

  // Append path2 to path1.
  size_t path1_len = strlen(path1);
  size_t path2_len = strlen(path2);
  size_t path_len = path1_len + path2_len + 2;
  char *path = new char[path_len];
  for (size_t i = 0; i < path1_len; i++) { path[i] = path1[i]; }
  path[path1_len] = '/';
  for (size_t i = 0; i <= path2_len; i++) {
    path[i + path1_len + 1U] = path2[i];
  }
  return path;
}

/*
 * Uses strcmp to compare the given strings. Returns true if they are equal
 * and non-null
 */
bool StrEq(const char *str1, const char *str2) {
  return (str1 != NULL) && (str2 != NULL) && (strcmp(str1, str2) == 0);
}

/*
 * Copies the given string into a newly allocated string.
 *
 * Assumes the given string is non-null.
 */
char *StrCpy(const char *str) {
  size_t len = strlen(str);
  char *copy = new char[len + 1];
  for (size_t i = 0; i <= len; i++) { copy[i] = str[i]; }
  return copy;
}
