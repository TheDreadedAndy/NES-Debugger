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
#include "../util/contracts.h"
#include "../util/util.h"
#include "../ppu/palette.h"
#include "../ndb.h"

// Window size constants
#define NES_WIDTH 256
#define NES_HEIGHT 240
#define MAX_TITLE_SIZE 256

// The name displayed in the SDL window.
const char *window_name = "NES, I guess?";

// Global sdl window variable, used to render to the game window, play sounds,
// and collect input. Cannot be directly accessed outside this file.
SDL_Window *window = NULL;

// Global SDL surface that pixels are rendered to before being drawn in the
// window. Cannot be directly accessed outside this file.
SDL_Surface *render = NULL;

/* Helper functions */
void window_process_window_event(SDL_Event *event);

/*
 * Sets up the SDL window for rendering and gathering input.
 */
bool window_init(void) {
  // Init SDL's video system.
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "Failed to initialize SDL.\n");
    return false;
  }

  // Create window.
  window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, NES_WIDTH, NES_HEIGHT,
                            SDL_WINDOW_RESIZABLE);

  // Check if the window was created successfully.
  if (window == NULL) {
    fprintf(stderr, "Failed to create SDL window.\n");
    return false;
  }

  // Create the rendering surface.
  render = SDL_CreateRGBSurface(0, NES_WIDTH, NES_HEIGHT, PALETTE_DEPTH,
                                PALETTE_RMASK, PALETTE_GMASK, PALETTE_BMASK, 0);

  // Verify that the surface was created successfully.
  if (render == NULL) {
    fprintf(stderr, "Failed to create SDL rendering surface.\n");
    return false;
  }

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

  SDL_DestroyWindow(window);
  SDL_FreeSurface(render);
  SDL_Quit();

  return;
}
