#ifndef _NES_SIG
#define _NES_SIG

// Determines if the program is still running.
// Setting to false closes the program.
extern bool ndb_running;

// Used to signal that the configuration should be reloaded.
// Setting to true causes a reload. Set to true whenever a SIGUSR1 is received.
extern bool reload_config;

// Registers the signal handlers associated with the above variables.
void RegisterSignalHandlers(void);

#endif
