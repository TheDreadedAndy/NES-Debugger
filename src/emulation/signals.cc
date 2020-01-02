/*
 * TODO
 */

#include "./signals.h"

#include <cstdlib>

#include <signal.h>

// Signal variables, used to respond to signals at a pre-determined time.
bool ndb_running = true;
bool reload_config = false;

/* Signal handler function declaration */
void HandleStopSignal(int sig);
void HandleReloadSignal(int sig);

/*
 * Registers the signal handlers defined in this file.
 */
void RegisterSignalHandlers(void) {
  // Prepare the sigaction structure which will be used to register the signals.
  struct sigaction action;
  sigemptyset(&(action.sa_mask));
  sigaddset(&(action.sa_mask), SIGUSR1);
  sigaddset(&(action.sa_mask), SIGTERM);
  sigaddset(&(action.sa_mask), SIGINT);
  action.sa_flags = 0;

  // Register the SIGTERM handler.
  action.sa_handler = &HandleStopSignal;
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGINT, &action, NULL);

  // Register the SIGUSR1 handler.
  action.sa_handler = &HandleReloadSignal;
  sigaction(SIGUSR1, &action, NULL);

  return;
}

/*
 * Handles a SIGTERM signal, which sets the ndb_running flag to false.
 */
void HandleStopSignal(int sig) {
  (void)sig;
  ndb_running = false;
  return;
}

/*
 * Handles a SIGUSR1 signal, which sets the reload_config flag to true.
 */
void HandleReloadSignal(int sig) {
  (void)sig;
  reload_config = true;
  return;
}
