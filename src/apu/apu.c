/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "./apu.h"
#include "../util/util.h"
#include "../util/data.h"
#include "../cpu/2A03.h"

/*
 * Contains the data related to the operation of an APU pulse channel.
 */
typedef struct pulse {
  dword_t timer;
  word_t length;
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
  word_t period;
  word_t length;
  word_t control;
} noise_t;

/*
 * Contains the data related to the operation of the APU DMC channel.
 */
typedef struct dmc {
  // Memory mapped registers.
  word_t control;
  word_t rate;
  word_t level;
  dword_t addr;
  dword_t length;

  // Internal registers.
  dword_t current_addr;
  dword_t bytes_remaining;
  word_t bits_remaining;
  word_t sample_buffer;
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

// These flags can be used to control the individual channels.
#define FLAG_PULSE_HALT 0x20U
#define FLAG_TRI_HALT 0x80U
#define FLAG_NOISE_HALT 0x20U

// There are 3729 APU clock cycles per APU frame step.
#define FRAME_STEP_LENGTH 3729U

// Some registers are mapped in the CPU memory space. These are their addresses.
#define PULSE_A_CONTROL_ADDR 0x4000U
#define PULSE_A_SWEEP_ADDR 0x4001U
#define PULSE_A_TIMERL_ADDR 0x4002U
#define PULSE_A_LENGTH_ADDR 0x4003U
#define PULSE_B_CONTROL_ADDR 0x4004U
#define PULSE_B_SWEEP_ADDR 0x4005U
#define PULSE_B_TIMERL_ADDR 0x4006U
#define PULSE_B_LENGTH_ADDR 0x4007U
#define TRI_CONTROL_ADDR 0x4008U
#define TRI_TIMERL_ADDR 0x400AU
#define TRI_LENGTH_ADDR 0x400BU
#define NOISE_CONTROL_ADDR 0x400CU
#define NOISE_PERIOD_ADDR 0x400EU
#define NOISE_LENGTH_ADDR 0x400FU
#define DMC_CONTROL_ADDR 0x4010U
#define DMC_COUNTER_ADDR 0x4011U
#define DMC_ADDRESS_ADDR 0x4012U
#define DMC_LENGTH_ADDR 0x4013U
#define APU_STATUS_ADDR 0x4015U
#define FRAME_COUNTER_ADDR 0x4017U

// MMIO writes may change multiple values using different bits. These masks
// allow for this.
#define LENGTH_MASK 0xF8U
#define LENGTH_SHIFT 3U
#define TIMER_HIGH_MASK 0x07U
#define TIMER_HIGH_SHIFT 8U
#define TIMER_LOW_MASK 0xFFU

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
 * Performs an APU frame step action according to the current step and mode
 * of the frame counter.
 *
 * Assumes the APU has been initialized.
 */
void apu_run_frame_step(void) {
  // Clock envelopes.
  // TODO

  // Clock length counters.
  if ((frame_step == 1) || ((frame_step == 3) && !(frame_control & FLAG_MODE))
                        || ((frame_step == 4) && (frame_control & FLAG_MODE))) {
    // Decrement pulse A length counter.
    if ((pulse_a->length > 0) && !(pulse_a->control & FLAG_PULSE_HALT)) {
      pulse_a->length--;
    }

    // Decrement pulse B length counter.
    if ((pulse_b->length > 0) && !(pulse_b->control & FLAG_PULSE_HALT)) {
      pulse_b->length--;
    }

    // Decrement triangle length counter.
    if ((triangle->length > 0) && !(triangle->control & FLAG_TRI_HALT)) {
      triangle->length--;
    }

    // Decrement noise length counter.
    if ((noise->length > 0) && !(noise->control & FLAG_NOISE_HALT)) {
      noise->length--;
    }
  }

  // Send the frame IRQ. The IRQ should only be added to the line if it is not
  // already there.
  if ((frame_step == 3) && !(frame_control & (FLAG_MODE | FLAG_IRQ_DISABLE))
                        && !frame_irq) {
    frame_irq = true;
    irq_line++;
  }

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
  // Determine which register is being accessed.
  pulse_t *pulse;
  switch(reg_addr) {
    case PULSE_A_CONTROL_ADDR:
    case PULSE_B_CONTROL_ADDR:
      // Update the control and envelope register.
      pulse = (reg_addr == PULSE_A_CONTROL_ADDR) ? pulse_a : pulse_b;
      pulse->control = val;
      break;
    case PULSE_A_SWEEP_ADDR:
    case PULSE_B_SWEEP_ADDR:
      break;
    case PULSE_A_TIMERL_ADDR:
    case PULSE_B_TIMERL_ADDR:
      break;
    case PULSE_A_LENGTH_ADDR:
    case PULSE_B_LENGTH_ADDR:
      // Update the pulse length and timer high.
      pulse = (reg_addr == PULSE_A_LENGTH_ADDR) ? pulse_a : pulse_b;
      pulse->length = (val & LENGTH_MASK) >> LENGTH_SHIFT;
      // TODO: Other effects.
      break;
    case TRI_CONTROL_ADDR:
      // Update the linear control register.
      triangle->control = val;
      // TODO: Other effects.
      break;
    case TRI_TIMERL_ADDR:
      break;
    case TRI_LENGTH_ADDR:
      // Update the triangle length and timer high.
      triangle->length = (val & LENGTH_MASK) >> LENGTH_SHIFT;
      // TODO: Other effects.
      break;
    case NOISE_CONTROL_ADDR:
      // Update the noise envelope and control register.
      noise->control = val;
      break;
    case NOISE_PERIOD_ADDR:
      break;
    case NOISE_LENGTH_ADDR:
      // Update the noise length counter.
      noise->length = (val & LENGTH_MASK) >> LENGTH_SHIFT;
      break;
    case DMC_CONTROL_ADDR:
      break;
    case DMC_COUNTER_ADDR:
      break;
    case DMC_ADDRESS_ADDR:
      break;
    case DMC_LENGTH_ADDR:
      break;
    case APU_STATUS_ADDR:
      break;
    case FRAME_COUNTER_ADDR:
      // TODO: Add reset delay.
      frame_control = val;
      // Clear the frame IRQ if the disable flag was set by the write.
      if ((frame_control & FLAG_IRQ_DISABLE) && frame_irq) {
        frame_irq = false;
        irq_line--;
      }
      // Reset the frame timer.
      frame_clock = 0;
      frame_step = 0;
      break;
    default:
      // If the address is invalid, nothing happens.
      break;
  }

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
      irq_line--;
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
