#ifndef _NES_TIME
#define _NES_TIME

#include <cstdlib>
#include <ctime>

#include "../sdl/window.h"

// The frame rate of the NES.
#define NES_FRAME_RATE 60L

// The number of nanoseconds in a second.
#define NSECS_PER_SEC (1000000000L)

// Ensures that the caller does not run more than the specified number of
// times per second.
void EmutimeSyncFrameRate(long tic_rate);

// Updates the frame rate counter in the SDL window once per second, assuming
// the program is supposed to run at tic_rate frames per second.
void EmutimeUpdateFrameCounter(long tic_rate, Window *window);

#endif
