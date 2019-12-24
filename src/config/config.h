#ifndef _NES_CONFIG
#define _NES_CONFIG

#include <cstdlib>
#include <cstdint>
#include <cstdio>

/* Keys/vals for video configuration */

const char* const kRendererTypeKey = "renderer_type";
const char* const kRendererHardwareVal = "hardware";
const char* const kRendererSurfaceVal = "surface";

//TODO: Unused.
const char* const kVideoTypeKey = "video_type";
const char* const kVideoRGBVal = "RGB";
const char* const kVideoNTSCVal = "NTSC";

const char* const kPaletteFileKey = "palette_file";

/* Keys for controller configuration */

//TODO: Unused.
const char* const kControllerTypeKey = "controller_type";
const char* const kControllerDefaultVal = "default";
const char* const kControllerStandardVal = "standard";

const char* const kButtonAKey = "button_a";
const char* const kButtonBKey = "button_b";
const char* const kButtonStartKey = "button_start";
const char* const kButtonSelectKey = "button_select";
const char* const kButtonUpKey = "button_up";
const char* const kButtonDownKey = "button_down";
const char* const kButtonLeftKey = "button_left";
const char* const kButtonRightKey = "button_right";

/*
 * Maintains the current configuration for the emulation.
 * Configuration can be read from/written to a file in a pre-defined
 */
class Config {
  private:
    // The name of the config file, and its full subfolder path from
    // the users home/documents folder.
    const char* const kConfName_ = "ndb.conf";
    const char* const kLinuxSubFolder_ = "/.config/ndb/ndb.conf";
    const char* const kWinSubFolder_ = "/ndb/ndb.conf";

    // The configuration is stored as a dictionary of strings.
    struct DictElem {
      char *key;
      char *val;
      DictElem *next;
    };

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
    void WriteElem(DictElem *elem, FILE *config);

    // Hashes the given string.
    size_t Hash(const char *string);

  public:
    // Creates a config structure by loading from the given file
    // (or some defined location).
    Config(const char *config_file);

    // Completely reloads the data in the config class from the given file.
    void Load(const char *config_file = NULL);

    // Writes any changes back to the config file.
    void Save(const char *config_file = NULL);

    // Gets a field from the loaded configuration file.
    // If the field does not exist, the specified default value is returned.
    char *Get(const char* key, const char *default_value = NULL);

    // Sets a field in the loaded configuration.
    void Set(const char* key, const char *val);

    // Writes the config file, then deletes the config object.
    ~Config(void);
};

#endif
