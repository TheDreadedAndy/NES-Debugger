#ifndef _NES_CONFIG
#define _NES_CONFIG

#include <cstdint>

const char *kRendererTypeKey_ = "renderer_type"
const char *kVideoTypeKey_ = "video_type"
const char *kPaletteFileKey_ = "palette_file"
const char *kControllerTypeKey_ = "controller_type"

/*
 * Maintains the current configuration for the emulation.
 * Configuration can be read from/written to a file in a pre-defined
 */
class Config {
  private:
    // The configuration is stored as a dictionary of strings.
    typedef struct {
      char *key;
      char *val;
      DictElem *next;
    } DictElem;

    // The size of the dictionary is defined in the implementation.
    DictElem **dict_ = NULL;

    // Scans a key/value from a line in the configuration file.
    bool ScanKey(char *buf, size_t buf_size, FILE *config);
    bool ScanVal(char *buf, size_t buf_size, FILE *config);

    // Hashes the given string.
    size_t Hash(char *string);

  public:
    // Creates a config structure by loading from the given file
    // (or some defined location).
    Config(char *config_file);

    // Completely reloads the data in the config class from the given file.
    void Load(char *config_file);

    // Writes any changes back to the config file.
    void Save(void);

    // Gets a field from the loaded configuration file.
    // If the field does not exist, the specified default value is returned.
    char *Get(char *key, char *default_value = NULL);

    // Sets a field in the loaded configuration.
    void Set(char *key, char *val);

    // Writes the config file, then deletes the config object.
    ~Config(void);
};

#endif
