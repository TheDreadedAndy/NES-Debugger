/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "./apu.h"
#include "../util/util.h"
#include "../util/data.h"

/*
 * Contains the data related to the operation of an APU pulse channel.
 */
typedef struct pulse {
  dword_t timer;
  word_t length;
  word_t envelope;
  word_t sweep;
  word_t control;
} pulse_t;

/*
 * Contains the data related to the operation of the APU triangle channel.
 */
typedef struct triangle {
  dword_t timer;
  word_t length;
  word_t linear;
  word_t control;
} triangle_t;

/*
 * Contains the data related to the operation of the APU noise channel.
 */
typedef struct noise {
  dword_t timer;
  word_t length;
  word_t envelope;
  word_t linear;
  word_t control;
} noise_t;

/*
 * Contains the data related to the operation of the APU DMC channel.
 */
typedef struct dmc {
  word_t control;
  word_t rate;
  word_t level;
  dword_t addr;
  dword_t current_addr;
  dword_t length;
  dword_t bytes_remaining;
} dmc_t;

// These flags can be used to access the APU status.
#define FLAG_DMC_IRQ 0x80U
#define FLAG_FRAME_IRQ 0x40U
#define FLAG_DMC_ACTIVE 0x10U
#define FLAG_NOISE_ACTIVE 0x08U
#define FLAG_TRI_ACTIVE 0x04U
#define FLAG_PULSE_B_ACTIVE 0x02U
#define FLAG_PULSE_A_ACTIVE 0x01U

// These flags can be used to access the APU frame control register.
#define FLAG_MODE 0x80U
#define FLAG_IRQ_DISABLE 0x40U

// There are 3729 APU clock cycles per APU frame step.
#define FRAME_STEP_LENGTH 3729U

// Some registers are mapped in the CPU memory space. These are their addresses.
#define APU_STATUS_ADDR 0x4015U

/*
 * These global variables control the different channels of the APU.
 * They are accessed through MMIO, and otherwise unavailable outside
 * of this file.
 */
static pulse_t *pulse_a = NULL;
static pulse_t *pulse_b = NULL;
static triangle_t *triangle = NULL;
static noise_t *noise = NULL;
static dmc_t *dmc = NULL;
static word_t frame_control = 0;

/*
 * Both the frame counter and the DMC unit can generate an IRQ.
 * These variables track if either is currently doing so.
 */
static bool dmc_irq = false;
static bool frame_irq = false;

/*
 * The APU frame counter is clocked every 3729 clock cycles.
 * It can be placed in a 4 or 5 step mode, with each step performing some
 * defined action.
 */
static size_t frame_clock = 0;
static size_t frame_step = 0;

/* Helper functions. */
void apu_run_frame_step(void);
void apu_inc_frame(void);
void apu_update_pulse_a(void);
void apu_update_pulse_b(void);
void apu_update_triangle(void);
void apu_update_noise(void);
void apu_update_dmc(void);

/*
 * Initializes the APU structures.
 */
void apu_init(void) {
  // Allocate the channels of the APU.
  pulse_a = xcalloc(1, sizeof(pulse_t));
  pulse_b = xcalloc(1, sizeof(pulse_t));
  triangle = xcalloc(1, sizeof(triangle_t));
  noise = xcalloc(1, sizeof(noise_t));
  dmc = xcalloc(1, sizeof(dmc_t));
  return;
}

/*
 * Runs a cycle of the APU emulation.
 *
 * Assumes the APU has been initialized.
 */
void apu_run_cycle(void) {
  // If we're on the start of a frame step, run the frame counters action
  // for that step.
  if (frame_clock == 0) { apu_run_frame_step(); }

  // Increment the frame clock and step.
  apu_inc_frame();

  // Update the channels for this cycle.
  apu_update_pulse_a();
  apu_update_pulse_b();
  apu_update_triangle();
  apu_update_noise();
  apu_update_dmc();

  return;
}

/*
 * TODO
 */
void apu_run_frame_step(void) {
  return;
}

/*
 * Increments the frame cycle and step counters according to the mode of the
 * frame counter.
 *
 * Assumes the APU has been initialized.
 */
void apu_inc_frame(void) {
  frame_clock++;

  // Reset the clock if a step has been completed.
  if (frame_clock > FRAME_STEP_LENGTH) {
    frame_clock = 0;
    frame_step++;
  }

  // Reset the step if a frame has been completed for the given mode.
  if ((frame_step >= 4) || ((frame_step >= 5) && (frame_control & FLAG_MODE))) {
    frame_step = 0;
  }

  return;
}

/*
 * TODO
 */
void apu_update_pulse_a(void) {
  return;
}

/*
 * TODO
 */
void apu_update_pulse_b(void) {
  return;
}

/*
 * TODO
 */
void apu_update_triangle(void) {
  return;
}

/*
 * TODO
 */
void apu_update_noise(void) {
  return;
}

/*
 * TODO
 */
void apu_update_dmc(void) {
  return;
}

/*
 * Writes the given value to a memory mapped APU register.
 * Writes to invalid addresses are ignored.
 *
 * Assumes the APU has been initialized.
 */
void apu_write(dword_t reg_addr, word_t val) {
  (void)reg_addr;
  (void)val;
  return;
}

/*
 * Reads from a memory mapped APU register.
 * All registers, except the status register, are write only and return 0.
 *
 * Assumes the APU has been initialized.
 */
word_t apu_read(dword_t reg_addr) {
  if (reg_addr == APU_STATUS_ADDR) {
    // Place the status of the APU in a word, and return it.
    word_t status = 0;
    if (dmc_irq) { status |= FLAG_DMC_IRQ; }
    if (frame_irq) {
      // Reading the status register clears the frame IRQ flag.
      status |= FLAG_FRAME_IRQ;
      frame_irq = false;
    }
    if (dmc->bytes_remaining) { status |= FLAG_DMC_ACTIVE; }
    if (noise->length) { status |= FLAG_NOISE_ACTIVE; }
    if (triangle->length) { status |= FLAG_TRI_ACTIVE; }
    if (pulse_b->length) { status |= FLAG_PULSE_B_ACTIVE; }
    if (pulse_a->length) { status |= FLAG_PULSE_A_ACTIVE; }
    return status;
  } else {
    // Invalid register.
    return 0;
  }
}

/*
 * Frees the APU structures.
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
