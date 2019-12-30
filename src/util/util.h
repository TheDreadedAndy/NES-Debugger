#ifndef _NES_UTIL
#define _NES_UTIL

#include <cstdlib>
#include <cstdio>

#include "./data.h"

// Constants used to determine the directory where the emulator will store
// its persistent files.
const char* const kUndefinedRootFolder = "";
const char* const kLinuxSubPath = ".config/ndb";
const char* const kWindowsSubPath = "ndb";

// Folders are seperated differently on windows, because why wouldn't they be.
#ifdef _NES_OSWIN
const char kSlash = '\\';
#else
const char kSlash = '/';
#endif

// Returns a randomized array of type word_t. The array is created with new,
// and must be free'd with delete.
DataWord *RandNew(size_t size);

// Returns the size of the given file.
size_t GetFileSize(FILE *file);

// Prompts the user to open a file, and then opens the file into the
// given pointer.
void OpenFile(FILE **file);

// Attempts to create all missing folders in the given path.
bool CreatePath(const char *str);

// Gets the configuration directory for the emulator.
char *GetRootFolder(void);

// Appends two strings to each other, adding a kSlash between them.
char *JoinPaths(const char *path1, const char *path2);

// Compares two null-terminated strings and returns true if they are equal.
bool StrEq(const char *str1, const char *str2);

// Copies a null terminated string into a newly allocated string.
char *StrCpy(const char *str);

#endif
