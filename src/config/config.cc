/*
 * TODO
 */

#include "./config.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifdef _NES_OSWIN
#include <windows.h>
#endif

#include "../util/util.h"

// The size of the configuration dictionary.
#define DICT_SIZE 64U

// The maximum size of key/val string that can be scanned in from
// the configuration file.
#define BUF_LIMIT 256U

/*
 * Creates a new configuration object by allocating a dictionary and
 * using the given configuration file to set values in it.
 */
Config::Config(const char *config_file) {
  // Creates an empty dictionary to hold the configuration.
  dict_ = new DictElem*[DICT_SIZE]();

  // Holds the default configuration file location.
  default_config_ = GetDefaultFile();

  // Loads the given configuration file into the dictionary.
  Load(config_file);

  return;
}

/*
 * Gets the default configuration files location for the users OS.
 * On Windows, this will be C:/Users/USER/My Documents/ndb/ndb.conf.
 * On Linux, this will be /home/USER/.config/ndb/ndb.conf
 *
 * This function always returns some valid string, though the path
 * may be inaccessible.
 *
 * The returned string must be deleted after use.
 */
char *Config::GetDefaultFile(void) {
#if defined(_NES_OSWIN)
  // Attempts to retrieve the location of the users documents folder.
  char path[MAX_PATH + 1];
  memset(path, 0, MAX_PATH);
  HRESULT res = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, -1,
                                SHGFP_TYPE_CURRENT, path);

  // If the users documents folder could no be obtained, we return the name
  // of the conf file instead.
  if (res != S_OK) {
    fprintf(stderr, "Error: Failed to find default config folder\n");
    return StrCpy(kConfName_);
  }

  // Otherwise, append the subfolder path to the users folder.
  size_t path_len = strlen(path);
  size_t sub_len = sizeof(kWinSubFolder_) - 1;
  char *conf = new char[path_len + sub_len + 1];
  for (size_t i = 0; i < path_len; i++) { conf[i] = path[i]; }
  for (size_t i = 0; i < sub_len; i++) {
    conf[path_len + i] = kWinSubFolder_[i];
  }
  conf[path_len + sub_len] = '\0';
  return conf;

#elif defined(_NES_OSLIN)
  // Gets the users home folder and appends the subfolder path to it.
  char *path = getenv("HOME");
  size_t path_len = strlen(path);
  size_t sub_len = sizeof(kLinuxSubFolder_) - 1;
  char *conf = new char[path_len + sub_len + 1];
  for (size_t i = 0; i < path_len; i++) { conf[i] = path[i]; }
  for (size_t i = 0; i < sub_len; i++) {
    conf[path_len + i] = kLinuxSubFolder_[i];
  }
  conf[path_len + sub_len] = '\0';
  return conf;

#else
  // If no OS is defined, we return the conf file name instead.
  return StrCpy(kConfName_);

#endif
}

/*
 * Loads the keys and values from the given configuration file into the
 * dictionary for the calling object.
 *
 * If the configuration file is null, a default is loaded.
 */
void Config::Load(const char *config_file) {
  // Attempts to open the given configuration file.
  FILE *config;
  if (config_file != NULL) {
    config = fopen(config_file, "a+");
    if (config == NULL) {
      config_file = NULL;
      fprintf(stderr, "Error: Failed to open the given config file\n");
    }
  }

  // If no file is given, or it cannot be opened, attempts to open a default.
  if (config_file == NULL) {
    config = fopen(default_config_, "a+");
    if (config == NULL) {
      fprintf(stderr, "Error: Failed to open the default config file\n");
      return;
    }
  }

  // Scans each line of the file for a valid configuration setting.
  // Keys and values can be no larger than BUF_LIMIT characters.
  char key[BUF_LIMIT];
  char val[BUF_LIMIT];
  char next_byte;
  while ((next_byte = fgetc(config)) != EOF) {
    // Undo the stream position increment from the loop guard.
    fseek(config, -1, SEEK_CUR);

    // Scan the next line for a key and value, and add them to the dictionary
    // if they're valid.
    if (ScanKey(key, BUF_LIMIT, config)) { continue; }
    if (ScanVal(val, BUF_LIMIT, config)) { continue; }
    Set(key, val);
  }

  // Closes the config file and returns.
  fclose(config);
  return;
}

/*
 * Scans the given file for a valid dictionary key on the current line.
 *
 * Fails if it finds and EOF or newline character before the assignment
 * character "=". Fails if the given buffer is filled before the string
 * is terminated.
 *
 * Returns false and jumps the file to the next line on failure.
 */
bool Config::ScanKey(char *buf, size_t buf_size, FILE *config) {
  // Scan the line until either a failing condition is met or the
  // entire key has been read.
  size_t i = 0;
  char next_byte;
  while ((next_byte = fgetc(config)) != '=') {
    // Add the next character to the buffer if it was valid.
    if ((next_byte == '\n') || (next_byte == EOF)) { return false; }
    buf[i] = next_byte;

    // Jump to the next line and return false if the buffer is now full.
    if (++i >= buf_size) {
      do {
        next_byte = fgetc(config);
      } while ((next_byte != '\n') && (next_byte != EOF));
      return false;
    }
  }

  // Terminate the string and return success.
  buf[i] = '\0';
  return true;
}

/*
 * Fills the buffer with data from the file.
 *
 * Assumes the file is positioned at the beginning of a key.
 *
 * Returns success if a newline or EOF is encountered before the buffer fills.
 * Returns failure if the buffer fills.
 */
bool Config::ScanVal(char *buf, size_t buf_size, FILE *config) {
  // Scan the line until either a stop character is encountered or
  // the buffer has been filled.
  size_t i = 0;
  char next_byte = fgetc(config);
  while ((next_byte != '\n') && (next_byte != EOF)) {
    // Add the character to the buffer and read the next character.
    buf[i] = next_byte;
    next_byte = fgetc(config);

    // Check if the buffer has been filled.
    if (++i >= buf_size) {
      do {
        next_byte = fgetc(config);
      } while ((next_byte != '\n') && (next_byte != EOF));
      return false;
    }
  }

  // Terminate the string and return failure.
  buf[i] = '\0';
  return true;
}

/*
 * Writes the current configuration to the given file.
 */
void Config::Save(const char *config_file) {
  // Attempts to open the given configuration file.
  FILE *config;
  if (config_file != NULL) {
    config = fopen(config_file, "a+");
    if (config == NULL) {
      config_file = NULL;
      fprintf(stderr, "Error: Failed to open the specified config file\n");
    }
  }

  // If the given config file was NULL, or if it could not be opened,
  // a default is used.
  if (config_file == NULL) {
    config = fopen(default_config_, "a+");
    if (config == NULL) {
      fprintf(stderr, "Error: Failed to open the default config file\n");
      return;
    }
  }

  // Writes each element to the opened config file.
  for (size_t i = 0; i < DICT_SIZE; i++) {
    DictElem *elem = dict_[i];
    while (elem != NULL) {
      WriteElem(elem, config);
      elem = elem->next;
    }
  }

  return;
}

/*
 * Writes an element from the dictionary to the given file.
 *
 * Assumes the given file is open and non-null.
 * Assumes the given element is non-null and valid.
 */
void Config::WriteElem(DictElem *elem, FILE *config) {
  fwrite(elem->key, 1, strlen(elem->key), config);
  fwrite("=", 1, 1, config);
  fwrite(elem->val, 1, strlen(elem->val), config);
  fwrite("\n", 1, 1, config);
  return;
}

/*
 * Attempts to get the value assigned to the given key in the configuration
 * dictionary. If the key is not in the dictionary, the given default value
 * is assigned set and returned. If no default is given, NULL is returned
 * and no value is set.
 *
 * Assumes the given key is non-null and non-empty.
 *
 * The returned value must not be deleted.
 */
char *Config::Get(const char *key, const char *default_value) {
  // Get the index of the key, and check if it's in the list.
  size_t index = Hash(key);
  DictElem *elem = dict_[index];
  DictElem *last_elem = NULL;
  while (elem != NULL) {
    if (StrEq(elem->key, key)) { return elem->val; }
    last_elem = elem;
    elem = elem->next;
  }

  // If the key was not in the dictionary, and a default was provided,
  // the default is added to the list and then returned.
  if (default_value != NULL) {
    // Checks if this element is the start of a new list.
    if (last_elem == NULL) {
      dict_[index] = new DictElem();
      elem = dict_[index];
    } else {
      // Otherwise, the element is added to the end of the list.
      last_elem->next = new DictElem();
      elem = last_elem->next;
    }

    // Creates copies of the key and value for the element, then returns
    // the value.
    elem->key = StrCpy(key);
    elem->val = StrCpy(default_value);
    return elem->val;
  }

  // Otherwise, NULL is returned as no value could be found.
  return NULL;
}

/*
 * Adds the given value to the configuration dictionary under the given key.
 * Copies of the given key and value are used in the dictionary.
 *
 * Assumes the given key and value are non-null and valid.
 */
void Config::Set(const char *key, const char *val) {
  // If the key is already in the configuration dictionary, the value
  // is updated.
  size_t index = Hash(key);
  DictElem *elem = dict_[index];
  DictElem *last_elem = NULL;
  while (elem != NULL) {
    if (StrEq(elem->key, key)) {
      delete[] elem->val;
      elem->val = StrCpy(val);
      return;
    }
    last_elem = elem;
    elem = elem->next;
  }

  // If the key is not already in the dictionary, then a new element is
  // created for it and its value is set.
  if (last_elem == NULL) {
    dict_[index] = new DictElem();
    elem = dict_[index];
  } else {
    last_elem->next = new DictElem();
    elem = last_elem->next;
  }
  elem->key = StrCpy(key);
  elem->val = StrCpy(val);

  return;
}

/*
 * Hashes the given string.
 *
 * Assumes the given string is non-null.
 */
size_t Config::Hash(const char *string) {
  // Start with prime numbers for a better distrobution.
  const size_t prime_start = 31U;
  size_t hash = 3U;

  // Use the prime numbers and the values of the characters in the string
  // to create a hash for the string.
  size_t i = 0;
  while (string[i] != '\0') {
    hash = hash * prime_start + string[i];
    i++;
  }
  return hash % DICT_SIZE;
}

/*
 * Deletes the dictionary and all of its elements.
 */
Config::~Config(void) {
  // Delete each list of elements.
  for (size_t i = 0; i < DICT_SIZE; i++) {
    DictElem *elem = dict_[i];
    DictElem *next_elem;
    while (elem != NULL) {
      next_elem = elem->next;
      delete[] elem->key;
      delete[] elem->val;
      delete elem;
      elem = next_elem;
    }
  }

  // Delete the dict itself and the default folder path string.
  delete[] dict_;
  delete[] default_config_;

  return;
}
