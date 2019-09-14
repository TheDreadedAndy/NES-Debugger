#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#ifndef _NES_TIME
#define _NES_TIME

// The frame rate of the NES.
#define NES_FRAME_RATE 60L

// The number of nanoseconds in a second.
#define NSECS_PER_SEC (1000000000L)

// Ensures that the caller does not run more than the specified number of
// times per second.
void emutime_sync_frame_rate(long tic_rate);

// Updates the frame rate counter in the SDL window once per second, assuming
// the program is supposed to run at tic_rate frames per second.
void emutime_update_frame_counter(long tic_rate);

#endif
