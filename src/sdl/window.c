/*
 * This file is used to initialize all SDL systems used by the emulator.
 * It is required that window_init be called before using any functions
 * in render.c, audio.c, and input.c. As such, the window should
 * be created before the emulation has started and closed after the emulation
 * has ended. If the initialization of SDL fails, the emulation should be
 * aborted.
 *
 * Direct calls to the SDL library should not be used outside of this file
 * and the three aforementioned files. Seperating calls in this way will
 * allow for the emulator to be easily switched away from SDL, should the
 * need be.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "./window.h"
#include "./input.h"
#include "./render.h"
#include "../util/contracts.h"
#include "../util/util.h"
#include "../ppu/palette.h"
#include "../ndb.h"

// Window size constants
#define NES_WIDTH 256
#define NES_TRUE_WIDTH 280
#define NES_HEIGHT 240
#define NES_TRUE_HEIGHT 224
#define WINDOW_WIDTH 560
#define WINDOW_HEIGHT 448
#define MAX_TITLE_SIZE 256

// The name displayed in the SDL window.
const char *window_name = "NES, I guess?";

// Global sdl window variable, used to render to the game window, play sounds,
// and collect input. Cannot be directly accessed outside this file.
SDL_Window *window = NULL;

// Global SDL surface that pixels are rendered to before being drawn in the
// window. Cannot be directly accessed outside this file.
SDL_Renderer *render = NULL;

/* Helper functions */
void window_process_window_event(SDL_Event *event);

/*
 * Sets up the SDL window for rendering and gathering input.
 */
bool window_init(void) {
  // Init SDL's video system.
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "Failed to initialize SDL.\n");
    return false;
  }

  // Create window.
  window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_RESIZABLE);

  // Check if the window was created successfully.
  if (window == NULL) {
    fprintf(stderr, "Failed to create SDL window.\n");
    return false;
  }

  // Create the renderer
  render = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

  // Verify that the renderer was created successfully.
  if (render == NULL) {
    fprintf(stderr, "Failed to create SDL renderer.\n");
    return false;
  }

  // Set the logical size of the renderer to the size of the NES output
  // resolution.
  SDL_RenderSetLogicalSize(render, NES_TRUE_WIDTH, NES_TRUE_HEIGHT);
  SDL_RenderSetViewport(render, NULL);

  // Return success.
  return true;
}

/*
 * Processes all events on the SDL event queue.
 *
 * Assumes that SDL has been initialized.
 */
void window_process_events(void) {
  // Loop over all events on the SDL event queue.
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Determine the event type and call its handling function.
    switch (event.type) {
      case SDL_WINDOWEVENT:
        window_process_window_event(&event);
        break;
      case SDL_KEYDOWN:
        input_press(event.key.keysym.sym);
        break;
      case SDL_KEYUP:
        input_release(event.key.keysym.sym);
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
 * Assumes that SDL has been initialized.
 */
void window_process_window_event(SDL_Event *event) {
  // Determine which window event is being thrown.
  switch (event->window.event) {
    case SDL_WINDOWEVENT_CLOSE:
      // The emulation window has been closed, and so the program should quit.
      ndb_running = false;
      break;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      // The window has been resized, so the surface must be marked as invalid.
      render_invalidate_window_surface();
      break;
    default:
      break;
  }

  return;
}

/*
 * Displays the current fps of the emulation in the window title.
 *
 * Assumes that SDL has been initialized.
 */
void window_display_fps(double fps) {
  char buf[MAX_TITLE_SIZE];
  sprintf(buf, "%s | FPS: %.1f", window_name, fps);
  SDL_SetWindowTitle(window, buf);
  return;
}

/*
 * Closes the SDL window.
 *
 * Assumes the window and render surface have been initialized.
 */
void window_close(void) {
  CONTRACT(window != NULL);
  CONTRACT(render != NULL);

  SDL_DestroyRenderer(render);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return;
}
