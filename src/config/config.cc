/*
 * TODO
 */

#include "./config.h"

#include <new>
#include <cstdlib>
#include <cstdint>
#include <cstdio>

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
Config::Config(char *config_file) {
  // Creates an empty dictionary to hold the configuration.
  dict_ = new DictElem[DICT_SIZE];

  // Loads the given configuration file into the dictionary.
  Load(config_file);

  return;
}

/*
 * Loads the keys and values from teh given configuration file into the
 * dictionary for the calling object.
 *
 * If the configuration file is null, a default is loaded.
 */
void Config::Load(char *config_file) {
  // Attempts to open the given configuration file.
  FILE *config;
  if (config_file == NULL) {
    config = fopen(kDefaultConfig_, "a+");
  } else {
    config = fopen(config_file, "a+");
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
    if ((key = ScanKey(key, BUF_LIMIT, config)) == NULL) { continue; }
    if ((val = ScanVal(val, BUF_LIMIT, config)) == NULL) { continue; }
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
 * TODO
 */
void Config::Save(void) {
  return;
}

/*
 * TODO
 */
char *Get(char *key, char *default_value = NULL) {
  return NULL;
}

/*
 * TODO
 */
size_t Hash(char *string) {
  return 0;
}

/*
 * TODO
 */
void Set(char *key, char *val) {
  return;
}

/*
 * TODO
 */
Config::~Config(void) {
  return;
}
