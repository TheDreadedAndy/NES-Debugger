#ifndef _NES_CONFIG
#define _NES_CONFIG

#include <cstdint>

/* Keys for video configuration */
const char *kRendererTypeKey = "renderer_type"
const char *kVideoTypeKey = "video_type"
const char *kPaletteFileKey = "palette_file"

/* Keys for controller configuration */
const char *kControllerTypeKey = "controller_type"

/*
 * Maintains the current configuration for the emulation.
 * Configuration can be read from/written to a file in a pre-defined
 */
class Config {
  private:
    // The name of the config file, and its full subfolder path from
    // the users home/documents folder.
    const char *kConfName_ = "ndb.conf";
    const char *kLinuxSubFolder = "/.config/ndb/ndb.conf";
    const char *kWinSubFolder = "/ndb/ndb.conf";

    // The configuration is stored as a dictionary of strings.
    typedef struct {
      char *key;
      char *val;
      DictElem *next;
    } DictElem;

    // The size of the dictionary is defined in the implementation.
    DictElem **dict_ = NULL;

    // The default location for the configuration file.
    // This value is OS dependent.
    char *default_config_;

    // Determines the default configuration location should be.
    // The implementation of this function is OS dependent.
    char *GetDefaultFile(void);

    // Scans a key/value from a line in the configuration file.
    bool ScanKey(char *buf, size_t buf_size, FILE *config);
    bool ScanVal(char *buf, size_t buf_size, FILE *config);

    // Writes an element of the config dictionary to the given file.
    void WriteElem(DictElem *elem, FILE *file);

    // Hashes the given string.
    size_t Hash(char *string);

  public:
    // Creates a config structure by loading from the given file
    // (or some defined location).
    Config(char *config_file);

    // Completely reloads the data in the config class from the given file.
    void Load(char *config_file);

    // Writes any changes back to the config file.
    void Save(char *config_file);

    // Gets a field from the loaded configuration file.
    // If the field does not exist, the specified default value is returned.
    char *Get(char *key, char *default_value = NULL);

    // Sets a field in the loaded configuration.
    void Set(char *key, char *val);

    // Writes the config file, then deletes the config object.
    ~Config(void);
};

#endif
