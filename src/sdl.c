#include <stdio.h>
#include <SDL2/SDL.h>

/*
 * Example SDL code, pulled from sdl docs (Mostly).
 */

int main(int argc, char* argv[]) {
  // Suppress GCC complaints.
  // These variables need to be here for SDL to work.
  (void)argc;
  (void)argv;

  // Init SDL's video system.
  SDL_Init(SDL_INIT_VIDEO);

  // Create window.
  SDL_Window *window;
  window = SDL_CreateWindow("NES Debugger", SDL_WINDOWPOS_CENTERED,
           SDL_WINDOWPOS_CENTERED, 256, 240, SDL_WINDOW_RESIZABLE);

  // Check for failure.
  if (window == NULL) {
    printf("SDL failed\n");
  }

  // Hold the window for 3 seconds.
  SDL_Delay(10000);

  // Close the window and exit.
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
