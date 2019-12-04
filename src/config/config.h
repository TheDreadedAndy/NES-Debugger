#ifndef _NES_CONFIG
#define _NES_CONFIG

// Enumerations used to configure video
typedef enum { RENDER_SOFTWARE, RENDER_HARDWARE } RendererType;
typedef enum { VIDEO_RGB, VIDEO_NTSC } VideoType;

// Video configuration structure.
typedef struct {
  RendererType renderer = RENDER_HARDWARE;
  VideoType video = VIDEO_RGB;
  char *palette_file = NULL;
} VideoConfig;

// Enumerations to configure input.
typedef enum { HEADER_DEFINED, NES_CONTROLLER } ControllerType;

// Input configuration structure.
typedef struct {
  // The type of controller represented with this structure.
  ControllerType type = HEADER_DEFINED;

  // The key mappings for the buttons on the controller.
  // Some mappings may be undefined for some controller types.
  SDL_Keycode button_a = SDLK_x;
  SDL_Keycode button_b = SDLK_z;
  SDL_Keycode button_start = SDLK_RETURN;
  SDL_Keycode button_select = SDLK_BACKSPACE;
  SDL_Keycode button_up = SDLK_UP;
  SDL_Keycode button_down = SDLK_DOWN;
  SDL_Keycode button_left = SDLK_LEFT;
  SDL_Keycode button_right = SDLK_RIGHT;
} InputConfig;

/*
 * Maintains the current configuration for the emulation.
 * Configuration can be read from/written to a file in a pre-defined
 * location.
 */
class Config {
};

#endif
