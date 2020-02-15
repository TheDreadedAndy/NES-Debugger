/*
 * TODO
 */

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <getopt.h>

#include "./emulation/signals.h"
#include "./emulation/emulation.h"
#include "./util/util.h"

/*
 * Loads in the users arguments and starts ndb.
 */
int main(int argc, char *argv[]) {
  // Global variables needed for getopt.
  extern char *optarg;

  // Long option array, used to parse input.
  struct option long_opts[] = {
    { "surface", 0, NULL, 's' },
    { "hardware", 0, NULL, 'h' },
    { "file", 1, NULL, 'f' },
    { "palette", 1, NULL, 'p' }
  };

  // Prepares the configuration object, which can be modified by the
  // command line input. The configuration folder is created before
  // creating a configuration object.
  char *root_path = GetRootFolder();
  CreatePath(root_path);
  delete[] root_path;
  Config *config = new Config(NULL);

  // Parses the users command line input.
  char *rom_file = NULL;
  signed char opt;
  while ((opt = getopt_long(argc, argv, "hf:p:s", long_opts, NULL)) != -1) {
    switch (opt) {
      case 'f':
        rom_file = optarg;
        break;
      case 'p':
        config->Set(kPaletteFileKey, optarg);
        break;
      case 's':
        config->Set(kRendererTypeKey, kRendererSurfaceVal);
        break;
      case 'h':
        config->Set(kRendererTypeKey, kRendererHardwareVal);
        break;
      default:
        printf("Usage: ndb -f <FILE>\n");
        delete config;
        exit(0);
    }
  }

  // Open the rom. Prompt the user to select one if they did not already provide
  // one.
  FILE *rom = NULL;
  if (rom_file != NULL) {
    rom = fopen(rom_file, "rb");
  } else {
    OpenFile(&rom);
  }

  // Verify that the users file was opened correctly.
  if (rom == NULL) {
    fprintf(stderr, "Failed to open the specified file.\n");
    abort();
  }

  // Create the object that will run the emulation.
  Emulation *emu = Emulation::Create(rom, config);

  // Close the rom file.
  fclose(rom);

  // Register the signal handlers that will be used to control the emulation.
  RegisterSignalHandlers();

  // Main emulation loop.
  emu->Run();

  // Saves any changes the user made to the config.
  config->Save();

  // Clean up any allocated memory.
  delete emu;
  delete config;

  return 0;
}
