#include <stdio.h>
#include <SDL2/SDL.h>

/*
 * Example SDL code, pulled from sdl docs (Mostly).
 */

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  SDL_Window *window;
  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("NES Debugger", SDL_WINDOWPOS_CENTERED,
           SDL_WINDOWPOS_CENTERED, 256, 240, SDL_WINDOW_RESIZABLE);

  if (window == NULL) {
    printf("SDL failed\n");
  }

  SDL_Delay(3000);

  SDL_DestroyWindow(window);

  SDL_Quit();
  return 0;
}
