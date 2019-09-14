/*
 * This file is used for timing the emulation and SDL polling
 * in the main loop. Times are obtained using clock_gettime, and
 * compared using the functions in this file.
 *
 * This file uses posix standard functions and is, thus, not portable.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "./emutime.h"
#include "../sdl/window.h"
#include "../util/contracts.h"

// For ease of use, the timespec structure is redefined here.
typedef struct timespec emutime_t;

/* Helper functions */
void emutime_diff(emutime_t *time1, emutime_t *time2, emutime_t *res);
void emutime_get(emutime_t *time);
bool emutime_gt(emutime_t *time1, emutime_t *time2);
void emutime_update(emutime_t *current_time, emutime_t *last_time, long wait);

/*
 * Ensures that the program waits at least 1 / tic_rate seconds between calls
 * to this function. Used to time emulation.
 *
 * Assumes the provided tic rate is non-zero.
 */
void emutime_sync_frame_rate(long tic_rate) {
  // Used to track when this function was last called.
  static emutime_t last_time = { .tv_sec = 0, .tv_nsec = 0 };

  // Determine the minimum time at which this function can return.
  long wait_nsecs = NSECS_PER_SEC / tic_rate;
  emutime_t wait_spec = { .tv_sec = 0, .tv_nsec = wait_nsecs };
  wait_spec.tv_sec = last_time.tv_sec;
  wait_spec.tv_nsec += last_time.tv_nsec;
  if (wait_spec.tv_nsec >= NSECS_PER_SEC) {
    wait_spec.tv_nsec -= NSECS_PER_SEC;
    wait_spec.tv_sec++;
  }

  // Pause the program until the minimum time has been reached.
  emutime_t current_time, diff;
  emutime_get(&current_time);
  if (emutime_gt(&wait_spec, &current_time)) {
    emutime_diff(&wait_spec, &current_time, &diff);
    nanosleep(&diff, NULL);
  }

  // Update the last call time.
  emutime_get(&last_time);

  return;
}

/*
 * Updates the SDL window with the current running frame rate every
 * tic_rate frames.
 *
 * This function may not display the correct frame rate if the tic rate
 * is changed to a lower number while frames are being counted.
 */
void emutime_update_frame_counter(long tic_rate) {
  // Counts how many frames have been run
  static long frames_counted = 0;
  frames_counted++;

  // Tracks how much time has passed between frames.
  static emutime_t last_time = { .tv_sec = 0, .tv_nsec = 0 };

  // Determine if it is time to update the frame rate display.
  emutime_t current_time, diff;
  if (frames_counted >= tic_rate) {
    // Update the frame rate display.
    emutime_get(&current_time);
    emutime_diff(&current_time, &last_time, &diff);
    float secs_passed = ((float) diff.tv_sec) + (((float) diff.tv_nsec)
                                              / ((float) NSECS_PER_SEC));
    window_display_fps(((float) frames_counted) / secs_passed);
    frames_counted = 0;

    // Update the last called time.
    emutime_get(&last_time);
  }

  return;
}

/*
 * Gets the current time and stores it in a time spec structure.
 */
void emutime_get(emutime_t *time) {
#ifdef _NES_OSLIN
  clock_gettime(CLOCK_MONOTONIC_RAW, time);
#else
  clock_gettime(CLOCK_MONOTONIC, time);
#endif
  return;
}

/*
 * Checks if the first time is greater than the second time.
 *
 * Assumes the timespec structures are non-null.
 */
bool emutime_gt(emutime_t *time1, emutime_t *time2) {
  return (time1->tv_sec > time2->tv_sec) || ((time1->tv_sec == time2->tv_sec)
                                         && (time1->tv_nsec > time2->tv_nsec));
}

/*
 * Subtracts the first time from the second time, storing the result in res.
 *
 * Assumes the first time is greater than the second time.
 */
void emutime_diff(emutime_t *time1, emutime_t *time2, emutime_t *res) {
  CONTRACT(emutime_gt(time1, time2));

  // Borrow from seconds in nanoseconds, if needed.
  if (time1->tv_nsec < time2->tv_nsec) {
    time1->tv_nsec += NSECS_PER_SEC;
    time1->tv_sec--;
  }

  // Calculate the difference.
  res->tv_sec = time1->tv_sec - time2->tv_sec;
  res->tv_nsec = time1->tv_nsec - time2->tv_nsec;

  return;
}
