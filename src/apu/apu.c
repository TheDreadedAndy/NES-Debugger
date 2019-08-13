/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "../util/util.h"
#include "../util/data.h"

/*
 * TODO
 */
typedef struct channel {
  dword_t timer;
  word_t counter;
  word_t control;
  word_t sweep;
  word_t linear;
} channel_t;

/*
 * TODO
 */
typedef struct dmc {
  word_t control;
  word_t counter;
  word_t addr;
  word_t length;
} dmc_t;

/*
 * These global variables control the different channels of the APU.
 * They are accessed through MMIO, and otherwise unavailable outside
 * of this file.
 */
static channel_t *pulse_a = NULL;
static channel_t *pulse_b = NULL;
static channel_t *triangle = NULL;
static channel_t *noise = NULL;
static dmc_t *dmc = NULL;

/*
 * TODO
 */
void apu_init(void) {
  // Allocate the channels of the APU.
  pulse_a = xcalloc(1, sizeof(channel_t));
  pulse_b = xcalloc(1, sizeof(channel_t));
  triangle = xcalloc(1, sizeof(channel_t));
  noise = xcalloc(1, sizeof(channel_t));
  dmc = xcalloc(1, sizeof(dmc_t));
  return;
}

/*
 * TODO
 */
void apu_free(void) {
  // Free the channels of the APU.
  free(pulse_a);
  free(pulse_b);
  free(triangle);
  free(noise);
  free(dmc);
  return;
}
