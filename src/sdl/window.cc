/*
 * This file contains the master SDL class, called Window. Any interactions
 * with SDL classes should be done through the window class, and only one
 * window class should exist at a given time. The main purpose of this class
 * is to allow the emulation code to be independent from the renderering,
 * audio, and input code. The window class collects all the interfaces related
 * to SDL and manages them.
 *
 * The window class provides methods to gain access to its current renderer,
 * audio player, and input objects. These methods allow these objects to be
 * passed through to the pieces of the emulation which require them.
 *
 * Note that the window class also manages the SDL event queue. The queue is
 * processed whenever the ProcessEvents() method is called. This method should
 * be called at least once per frame, as it is responsible for sending the
 * signals to the input object which cause it to update the input state.
 * ProcessEvents() additionally handles events related to the main SDL window,
 * such as closing and resizing the window.
 */

#include "./window.h"

#include <new>
#include <cstdio>
#include <cstdint>

#include <SDL2/SDL.h>

#include "./input.h"
#include "./renderer.h"
#include "./audio_player.h"
#include "../util/util.h"
#include "../emulation/signals.h"
#include "../config/config.h"

// Window size constants
#define NES_WIDTH 256
#define NES_HEIGHT 240
#define WINDOW_WIDTH 560
#define WINDOW_HEIGHT 448
#define MAX_TITLE_SIZE 256

// The name displayed in the SDL window.
const char *kWindowName = "NES, I guess?";

/*
 * Attempts to create a Window object. Returns NULL and cleans any data already
 * created on failure.
 */
Window *Window::Create(Config *config) {
  // Init SDL's video system.
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "Failed to initialize SDL.\n");
    return NULL;
  }

#ifdef _NES_OSLIN
  // Force the IBus IME to handle text composing.
  // Work around for a known SDL2 crash.
  SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");

  // Prevent the application from disabling compositing.
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

  // Create window.
  SDL_Window *window = SDL_CreateWindow(kWindowName, SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                        WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);

  // Check if the window was created successfully.
  if (window == NULL) {
    fprintf(stderr, "Failed to create SDL window.\n");
    SDL_Quit();
    return NULL;
  }

  // Attempt to create a renderer; error on failure.
  Renderer *renderer = Renderer::Create(window, config);
  if (renderer == NULL) {
    fprintf(stderr, "Failed to create a renderer for the SDL window.\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  // Attempt to create an Audio Player; error on failure.
  AudioPlayer *audio = AudioPlayer::Create();
  if (audio == NULL) {
    fprintf(stderr, "Failed to create an audio player.\n");
    delete renderer;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  // Open the input config file and setup input.
  Input *input = new Input(config);

  // Create a Window object and return it to the caller.
  return new Window(window, renderer, audio, input);
}

/*
 * Assigns the provided objects to the private variables of the object.
 *
 * Assumes all of the given objects are non-null and valid.
 */
Window::Window(SDL_Window *window, Renderer *renderer,
               AudioPlayer *audio, Input *input) {
  window_ = window;
  renderer_ = renderer;
  audio_ = audio;
  input_ = input;
  return;
}

/*
 * Processes all events on the SDL event queue.
 */
void Window::ProcessEvents(void) {
  // Loop over all events on the SDL event queue.
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Determine the event type and call its handling function.
    switch (event.type) {
      case SDL_WINDOWEVENT:
        ProcessWindowEvent(&event);
        break;
      case SDL_KEYDOWN:
        input_->Press(event.key.keysym.sym);
        break;
      case SDL_KEYUP:
        input_->Release(event.key.keysym.sym);
        break;
      default:
        break;
    }
  }

  return;
}

/*
 * Processes the window event stored within the given event.
 *
 * Assumes that the event holds a window event.
 */
void Window::ProcessWindowEvent(SDL_Event *event) {
  // Determine which window event is being thrown.
  switch (event->window.event) {
    case SDL_WINDOWEVENT_CLOSE:
      // The emulation window has been closed, and so the program should quit.
      ndb_running = false;
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      // The window has been resized, so the surface must be marked as invalid.
      renderer_->InvalidateWindowSurface();
      break;
    default:
      break;
  }

  return;
}

/*
 * Displays the current fps of the emulation in the window title.
 */
void Window::DisplayFps(double fps) {
  char buf[MAX_TITLE_SIZE];
  sprintf(buf, "%s | FPS: %.1f", kWindowName, fps);
  SDL_SetWindowTitle(window_, buf);
  return;
}

/*
 * Exposes the created renderer object to the caller.
 */
Renderer *Window::GetRenderer(void) {
  return renderer_;
}

/*
 * Exposes the created audio player object to the caller.
 */
AudioPlayer *Window::GetAudioPlayer(void) {
  return audio_;
}

/*
 * Exposes the created input object to the caller.
 */
Input *Window::GetInput(void) {
  return input_;
}

/*
 * Closes the SDL window and deletes its related objects.
 * Quits from SDL.
 */
Window::~Window(void) {
  // Free all the interface objects.
  delete renderer_;
  delete audio_;
  delete input_;

  // Close SDL.
  SDL_DestroyWindow(window_);
  SDL_Quit();

  return;
}
