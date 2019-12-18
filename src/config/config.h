#ifndef _NES_CONFIG
#define _NES_CONFIG

// The number of buttons currently mappable within the emulation.
#define NUM_MAPS 8

// Enumeration used to index into the button mapping and string array.
typedef enum { BUTTON_A, BUTTON_B, BUTTON_START, BUTTON_SELECT, BUTTON_UP,
               BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT } ButtonName;

// Enumerations used to configure video
typedef enum { RENDER_SOFTWARE, RENDER_HARDWARE } RendererType;
typedef enum { VIDEO_RGB, VIDEO_NTSC } VideoType;

// Enumerations to configure input.
typedef enum { HEADER_DEFINED, NES_CONTROLLER } ControllerType;

/*
 * Maintains the current configuration for the emulation.
 * Configuration can be read from/written to a file in a pre-defined
 * location.
 */
class Config {
  private:
    // Video setting keys, for use with the config file.
    const char *kRendererTypeKey_ = "renderer_type"
    const char *kVideoTypeKey_ = "video_type"
    const char *kPaletteFileKey_ = "palette_file"

    // Input setting keys, for use with the config file.
    const char *kButtonMapKeys_[NUM_MAPS] = { "button_a", "button_b",
                "button_start", "button_select", "button_up", "button_down",
                "button_left", "button_right" };
    const char *kControllerTypeKey_ = "controller_type"

    // Variables used to hold the current video settings.
    RendererType renderer_type_ = RENDER_SOFTWARE;
    VideoType video_type_ = VIDEO_RGB;
    char *palette_file_ = NULL;

    // Variables used to hold the current input settings.
    SDL_Keycode button_map_[NUM_MAPS] = { SDLK_x, SDLK_z, SDLK_RETURN,
                SDLK_BACKSPACE, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };
    ControllerType controller_type_ = HEADER_DEFINED;

  public:
    // Creates a config structure by loading from the given file
    // (or some defined location).
    Config(char *config_file);

    // Completely reloads the data in the config class from the given file.
    void Load(char *config_file);

    // Writes any changes back to the config file.
    void Save(void);

    // Functions used to get each field in the configuration.
    RendererType GetRendererType(void);
    VideoType GetVideoType(void);
    char *GetPaletteFile(void);
    SDL_Keycode GetButtonMap(ButtonName button);
    ControllerType GetControllerType(void);

    // Functions used to set each field in the configuration.
    void SetRendererType(RendererType renderer_type);
    void SetVideoType(VideoType video_type);
    void SetPaletteFile(char *palette_file);
    void SetButtonMap(ButtonName button, SDL_Keycode map);
    void SetControllerType(ControllerType controller);

    // Writes the config file, then deletes the config object.
    ~Config(void);
};

#endif
