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
#include "../util/contracts.h"

/* Helper functions */
void emutime_diff(emutime_t *time1, emutime_t *time2, emutime_t *res);

/*
 * Gets the current time and stores it in a time spec structure.
 */
void emutime_get(emutime_t *time) {
  clock_gettime(CLOCK_MONOTONIC_RAW, time);
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
 * Updates the given last_time using the current_time, assuming that the action
 * should be performed every wait nanoseconds. The updated time represents the
 * time at which the action should again be started. If it has been longer
 * than wait nanoseconds since the last_time, the current_time is stored in
 * the last_time instead.
 *
 * Assumes that the current time is more recent than the last time.
 */
void emutime_update(emutime_t *current_time, emutime_t *last_time, long wait) {
  CONTRACT(emutime_gt(current_time, last_time));

  // Convert the wait time into a timespec structure.
  emutime_t wait_spec;
  wait_spec.tv_sec = wait / NSECS_PER_SEC;
  wait_spec.tv_nsec = wait % NSECS_PER_SEC;

  // Check if it has been longer than 1/60 seconds since the last time.
  emutime_t diff;
  emutime_diff(current_time, last_time, &diff);
  if (emutime_gt(&diff, &wait_spec)) {
    last_time->tv_sec = current_time->tv_sec;
    last_time->tv_nsec = current_time->tv_nsec;
  }

  // Since the action lasted less than wait nanoseconds, we need only
  // wait until the full time specified has passed.
  last_time->tv_nsec += wait;
  if (last_time->tv_nsec > NSECS_PER_SEC) {
    last_time->tv_nsec -= NSECS_PER_SEC;
    last_time->tv_sec++;
  }

  return;
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
