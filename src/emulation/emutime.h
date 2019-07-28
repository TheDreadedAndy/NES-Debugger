#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#ifndef _NES_TIME
#define _NES_TIME

// The number of nanoseconds in 1 frame at 60 fps.
#define FRAME_LENGTH (16666666L)

// The number of nanoseconds in a second.
#define NSECS_PER_SEC (1000000000L)

// Time type used by the emulator. Should not be directly accessed outside
// of emutime.c.
typedef struct timespec emutime_t;

// Gets the current time and stores it in the given time structure.
void emutime_get(emutime_t *time);

// Returns true when time1 is greater than time2.
bool emutime_gt(emutime_t *time1, emutime_t *time2);

// Updates last_time to a time when an action should next be performed,
// assuming it should be performed 60 times a second.
void emutime_update(emutime_t *current_time, emutime_t *last_time, long wait);

#endif
