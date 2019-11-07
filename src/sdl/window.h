#ifndef _NES_SDL
#define _NES_SDL

#include <SDL2/SDL.h>

#include "./renderer.h"
#include "./audio_player.h"
#include "./input.h"

/*
 * Maintains all of the data necessary to interact with SDL.
 *
 * Only one Window object should exist at any given time.
 */
class Window {
  private:
    // The main sdl window for rendering.
    SDL_Window *window_;

    // SDL sub-interfaces, which are created from this window.
    Renderer *renderer_;
    AudioPlayer *audio_;
    Input *input_;

    // Processes the window events stored in a given SDL event.
    void ProcessWindowEvent(SDL_Event *event);

    Window(SDL_Window *window, Renderer *renderer,
           AudioPlayer *audio, Input *input);

  public:
    // Attempts to create a Window object. Returns NULL on failure.
    static Window *Create(char *input_cfg, RenderType rendering_type);

    // Processes all relevent events on the SDL event queue.
    void ProcessEvents(void);

    // Displays the given FPS in the main window title.
    void DisplayFps(double fps);

    // Provides the caller with a copy of the respective sub-interface.
    Renderer *GetRenderer(void);
    AudioPlayer *GetAudioPlayer(void);
    Input *GetInput(void);

    // Closes the SDL window.
    ~Window(void);
};

#endif
