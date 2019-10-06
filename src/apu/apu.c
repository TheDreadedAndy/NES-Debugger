/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include "./apu.h"
#include "../util/util.h"
#include "../util/data.h"
#include "../cpu/2A03.h"
#include "../memory/memory.h"
#include "../sdl/audio.h"

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
#define FLAG_ENV_LOOP 0x20U
#define FLAG_TRI_HALT 0x80U
#define FLAG_LINEAR_CONTROL 0x80U
#define FLAG_NOISE_HALT 0x20U
#define FLAG_NOISE_MODE 0x80U
#define FLAG_DMC_LOOP 0x40U
#define FLAG_CONST_VOL 0x10U

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
#define VOLUME_MASK 0x0FU
#define DMC_CONTROL_MASK 0xC0U
#define DMC_RATE_MASK 0x0FU
#define DMC_LEVEL_MASK 0x7FU
#define DMC_ADDR_SHIFT 6U
#define DMC_ADDR_BASE 0xC000U
#define DMC_LENGTH_SHIFT 4U
#define DMC_LENGTH_BASE 0x0001U
#define PULSE_DUTY_MASK 0xC0U
#define PULSE_DUTY_SHIFT 6U
#define PULSE_SEQUENCE_MASK 0x80U
#define PULSE_TIMER_MASK 0x07FFU
#define PULSE_SWEEP_ENABLE 0x80U
#define PULSE_SWEEP_COUNTER_MASK 0x70U
#define PULSE_SWEEP_COUNTER_SHIFT 4U
#define PULSE_SWEEP_SHIFT_MASK 0x07U
#define PULSE_SWEEP_NEGATE_MASK 0x08U
#define NOISE_PERIOD_MASK 0x0FU
#define ENV_DECAY_START 15U
#define LINEAR_MASK 0x7FU

// These constants are used to properly update the DMC channel.
#define DMC_CURRENT_ADDR_BASE 0x8000U
#define DMC_LEVEL_MAX 127U

/*
 * Contains the data related to the operation of an APU pulse channel.
 */
typedef struct pulse {
  // Memory mapped registers.
  dword_t timer;
  word_t length;
  word_t sweep;
  bool sweep_reload;
  word_t control;

  // Internal registers.
  word_t sweep_counter;
  word_t pos;
  dword_t clock;
  word_t output;
  word_t env_clock;
  word_t env_volume;
} pulse_t;

/*
 * Contains the data related to the operation of the APU triangle channel.
 */
typedef struct triangle {
  // Memory mapped registers.
  dword_t timer;
  word_t length;
  word_t control;
  bool linear_reload;

  // Internal registers.
  dword_t clock;
  word_t output;
  word_t linear;
  word_t pos;
} triangle_t;

/*
 * Contains the data related to the operation of the APU noise channel.
 */
typedef struct noise {
  // Memory mapped registers.
  word_t period;
  word_t length;
  word_t control;

  // Internal registers.
  dword_t shift;
  dword_t timer;
  dword_t clock;
  word_t env_clock;
  word_t env_volume;
  word_t output;
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
  word_t output;
  bool silent;

  // The DMC updates whenever this value is greater than the corresponding
  // rate value.
  size_t clock;
} dmc_t;

/*
 * The rate value in the DMC channel maps to a number of APU cycles to wait
 * between updates, which corresponds to a given frequency. These wait
 * times are stored in this array.
 */
static size_t dmc_rates[] = { 214, 190, 170, 160, 143, 127, 113, 107,
                              95, 80, 71, 64, 53, 42, 36, 27 };

/*
 * The period value of the noise channel is determined using a lookup table
 * of APU cycle wait timers. This table is given here.
 */
static dword_t noise_periods[] = { 2, 4, 8, 16, 32, 48, 64, 80, 101, 127, 190,
                                  254, 381, 508, 1017, 2034 };

/*
 * The pulse (square wave) channels have 4 duty cycle settings which change the
 * wave form output. Here these output settings are represented as bytes with
 * each 1 representing a cycle where the wave form is high.
 */
static word_t pulse_waves[] = { 0x40U, 0x60U, 0x78U, 0x9FU };

/*
 * The triangle wave channel loops over a 32 step sequence, which is defined
 * here.
 */
static word_t triangle_wave[] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3,
                                  2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                  12, 13, 14, 15 };

/*
 * The APU has a length lookup table, which it uses to translate the values
 * written to the channel length counters into actual cycle lengths.
 */
static word_t length_table[] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10,
                                 14, 12, 26, 14, 12, 16, 24, 18, 48, 20, 96,
                                 22, 192, 24, 72, 26, 16, 28, 32, 30 };

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
static word_t channel_status = 0;

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

// Most functions only update every other cycle. This flag tracks that.
static bool cycle_even = false;

/* Helper functions. */
static void apu_run_frame_step(void);
static void apu_update_sweep(pulse_t *pulse);
static dword_t apu_get_pulse_target(pulse_t *pulse);
static void apu_update_pulse_envelope(pulse_t *pulse);
static void apu_update_noise_envelope(void);
static void apu_update_triangle_linear(void);
static void apu_update_length(void);
static void apu_inc_frame(void);
static void apu_update_pulse(pulse_t *pulse);
static void apu_update_triangle(void);
static void apu_update_noise(void);
static void apu_update_noise_shift(void);
static void apu_update_dmc(void);
static void apu_status_write(word_t val);
static void apu_play_sample(void);

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

  // On power-up, the noise shifter is seeded with the value 1.
  noise->shift = 1;
  return;
}

/*
 * Runs a cycle of the APU emulation.
 *
 * Assumes the APU has been initialized.
 * Assumes CPU memory has been initialized.
 */
void apu_run_cycle(void) {
  // Only the triangle wave updates on odd cycles.
  if (!cycle_even) {
    apu_update_triangle();
    apu_play_sample();
    cycle_even = !cycle_even;
    return;
  }

  // Update the frame counter. Must be done in this order to prevent
  // issues with frame counter resets.
  apu_inc_frame();
  if (frame_clock == 0) { apu_run_frame_step(); }

  // Update the channels for this cycle.
  apu_update_pulse(pulse_a);
  apu_update_pulse(pulse_b);
  apu_update_triangle();
  apu_update_noise();
  apu_update_dmc();

  // Play a sample, if it is time to do so.
  apu_play_sample();

  // Toggle the cycle evenness.
  cycle_even = !cycle_even;

  return;
}

/*
 * Performs an APU frame step action according to the current step and mode
 * of the frame counter.
 *
 * Assumes the APU has been initialized.
 */
static void apu_run_frame_step(void) {
  // Clock envelopes.
  if (!((frame_step == 3) && (frame_control & FLAG_MODE))) {
    apu_update_pulse_envelope(pulse_a);
    apu_update_pulse_envelope(pulse_b);
    apu_update_noise_envelope();
    apu_update_triangle_linear();
  }

  // Clock length counters and pulse sweep.
  if ((frame_step == 1) || ((frame_step == 3) && !(frame_control & FLAG_MODE))
                        || ((frame_step == 4) && (frame_control & FLAG_MODE))) {
    apu_update_length();
    apu_update_sweep(pulse_a);
    apu_update_sweep(pulse_b);
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
 * Updates the envelope counter of a pulse channel.
 *
 * Assumes the provided channel is non-null.
 */
static void apu_update_pulse_envelope(pulse_t *pulse) {
  // Check if it is time to update the envelope.
  if (pulse->env_clock == 0) {
    // Reset the envelope clock.
    pulse->env_clock = pulse->control & VOLUME_MASK;
    // If the envelope has ended and should be looped, reset the volume.
    if ((pulse->env_volume == 0) && (pulse->control & FLAG_ENV_LOOP)) {
      pulse->env_volume = ENV_DECAY_START;
    } else if (pulse->env_volume > 0) {
      // Otherwise, decay the volume.
      pulse->env_volume--;
    }
  } else {
    pulse->env_clock--;
  }

  return;
}

/*
 * Updates the envelope counter of the noise channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_noise_envelope(void) {
  // Check if it is time to update the envelope.
  if (noise->env_clock == 0) {
    // Reset the envelope clock.
    noise->env_clock = noise->control & VOLUME_MASK;
    // If the envelope has ended and should be looped, reset the volume.
    if ((noise->env_volume == 0) && (noise->control & FLAG_ENV_LOOP)) {
      noise->env_volume = ENV_DECAY_START;
    } else if (noise->env_volume > 0) {
      // Otherwise, decay the volume.
      noise->env_volume--;
    }
  } else {
    noise->env_clock--;
  }

  return;
}

/*
 * Updates the linear counter of the triangle channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_triangle_linear(void) {
  // Reload the linear counter if the flag has been set.
  if (triangle->linear_reload) {
    triangle->linear = triangle->control & LINEAR_MASK;
  } else if (triangle->linear > 0) {
    // Otherwise, decrement it until it reaches zero.
    triangle->linear--;
  }

  // Clear the linear counter reload flag if the control bit is clear.
  if (!(triangle->control & FLAG_LINEAR_CONTROL)) {
    triangle->linear_reload = false;
  }

  return;
}

/*
 * Updates the length counter of each channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_length(void) {
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

  return;
}

/*
 * Updates a pulse channels period if the sweep counter is 0 and the channel
 * is not muted.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_sweep(pulse_t *pulse) {
  // Update the pulses period, if able.
  dword_t target_period = apu_get_pulse_target(pulse);
  if ((pulse->sweep_counter == 0) && (pulse->sweep & PULSE_SWEEP_ENABLE)
                                  && (pulse->timer >= 8)
                                  && (pulse->length > 0)
                                  && (target_period <= PULSE_TIMER_MASK)
                                  && (pulse->sweep & PULSE_SWEEP_SHIFT_MASK)) {
    pulse->timer = target_period;
  }

  // Update the sweep counter for the pulse.
  if ((pulse->sweep_counter == 0) || pulse->sweep_reload) {
    pulse->sweep_reload = false;
    pulse->sweep_counter = (pulse->sweep & PULSE_SWEEP_COUNTER_MASK)
                         >> PULSE_SWEEP_COUNTER_SHIFT;
  } else {
    pulse->sweep_counter--;
  }

  return;
}

/*
 * Calculates the sweep target period of the given pulse channel
 *
 * Assumes the pulse channel is non-null.
 */
static dword_t apu_get_pulse_target(pulse_t *pulse) {
  // Use the shift amount to get the period change form the timer,
  // then negate it if necessary.
  dword_t period_change = pulse->timer >> (pulse->sweep
                        & PULSE_SWEEP_SHIFT_MASK);
  if (pulse->sweep & PULSE_SWEEP_NEGATE_MASK) {
    // The two pulse channels subtract the period change in different ways.
    period_change = (pulse == pulse_a) ? ~period_change : -period_change;
    period_change = (period_change + pulse->timer) & PULSE_TIMER_MASK;
  } else {
    period_change += pulse->timer;
  }

  // Return the resulting change.
  return period_change;
}

/*
 * Increments the frame cycle and step counters according to the mode of the
 * frame counter.
 *
 * Assumes the APU has been initialized.
 */
static void apu_inc_frame(void) {
  frame_clock++;

  // Reset the clock if a step has been completed.
  if (frame_clock > FRAME_STEP_LENGTH) {
    frame_clock = 0;
    frame_step++;

    // Reset the step if a frame has been completed for the given mode.
    if ((frame_step >= 5) || ((frame_step >= 4)
                          && !(frame_control & FLAG_MODE))) {
      frame_step = 0;
    }
  }

  return;
}

/*
 * Updates the output of a pulse channel.
 *
 * Assumes the pulse channel is non-null.
 */
static void apu_update_pulse(pulse_t *pulse) {
  // Determine what the pulse channel is outputting audio on this cycle.
  word_t sequence = (pulse_waves[(pulse->control & PULSE_DUTY_MASK)
                  >> PULSE_DUTY_SHIFT] << pulse->pos) & PULSE_SEQUENCE_MASK;
  dword_t target_period = apu_get_pulse_target(pulse);
  if (sequence && (pulse->length > 0) && (pulse->timer >= 8)
               && (target_period <= PULSE_TIMER_MASK)) {
    pulse->output = (pulse->control & FLAG_CONST_VOL)
                  ? pulse->control & VOLUME_MASK
                  : pulse->env_volume;
  } else {
    pulse->output = 0;
  }

  // Check if it is time to update the pulse channel.
  if (pulse->clock > 0) {
    pulse->clock--;
  } else {
    pulse->clock = pulse->timer;
    // Increment the pulse sequence position.
    pulse->pos = (pulse->pos + 1) & 0x07U;
  }

  return;
}

/*
 * Updates the output of the triangle channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_triangle(void) {
  // Update the triangle waves output for this cycle. The triangle wave cannot
  // be silenced, it simply stops updating its position. However, the wave can
  // be given a frequency outside the human range of hearing, in which case we
  // force it to zero.
  triangle->output = (triangle->timer > 1) ? triangle_wave[triangle->pos] : 0U;

  // Update the triangle clock and output wave form using the timer period.
  if (triangle->clock > 0) {
    triangle->clock--;
  } else {
    triangle->clock = triangle->timer;
    // Determine if the position should be updated.
    if ((triangle->linear > 0) && (triangle->length > 0)) {
      triangle->pos = (triangle->pos + 1) & 0x1FU;
    }
  }

  return;
}

/*
 * Updates the output of the noise channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_noise(void) {
  // Determine what sound should be output this cycle.
  if ((noise->length > 0) && !(noise->shift & 0x01U)) {
    noise->output = (noise->control & FLAG_CONST_VOL)
                  ? noise->control & VOLUME_MASK
                  : noise->env_volume;
  } else {
    noise->output = 0;
  }

  // Check if it is time to update the noise channel.
  if (noise->clock > 0) {
    noise->clock--;
  } else {
    noise->clock = noise->timer;
    apu_update_noise_shift();
  }

  return;
}

/*
 * Updates the shift register of the noise channel.
 *
 * Assumes the APU has been initialized.
 */
static void apu_update_noise_shift(void) {
  // Calculate the new feedback bit of the noise channel using the noise mode.
  dword_t feedback;
  if (noise->period & FLAG_NOISE_MODE) {
    // In mode 1, the new bit is bit 0 XOR bit 6.
    feedback = (noise->shift & 0x01U) ^ ((noise->shift >> 6U) & 0x01U);
  } else {
    // In mode 0, the new bit is bit 0 XOR bit 1.
    feedback = (noise->shift & 0x01U) ^ ((noise->shift >> 1U) & 0x01U);
  }

  // Shift the noise shifter and backfill with the feedback bit.
  noise->shift = (feedback << 14U) | (noise->shift >> 1U);

  return;
}

/*
 * Updates the DMC channel, filling the buffer and updating the level as
 * necessary. May optionally generate an IRQ when a sample finishes.
 *
 * Assumes the APU has been initialized.
 * Assumes CPU memory has been initialized.
 */
static void apu_update_dmc(void) {
  // Check if it is time for the DMC channel to update.
  if (dmc->clock >= dmc_rates[dmc->rate]) {
    dmc->clock = 0;
  } else {
    dmc->clock++;
    return;
  }

  // Refill the sample buffer if it is empty.
  if ((dmc->bits_remaining == 0) && (dmc->bytes_remaining > 0)) {
    // TODO: Stall CPU.
    // Load the next word into the sample buffer.
    dmc->sample_buffer = memory_read(dmc->current_addr);
    dmc->current_addr = (dmc->current_addr + 1) | DMC_CURRENT_ADDR_BASE;
    dmc->bytes_remaining--;
    dmc->silent = false;

    if ((dmc->bytes_remaining == 0) && (dmc->control & FLAG_DMC_LOOP)) {
      dmc->current_addr = dmc->addr;
      dmc->bytes_remaining = dmc->length;
    } else if ((dmc->bytes_remaining == 0) && (dmc->control & FLAG_DMC_IRQ)
                                           && !dmc_irq) {
      // Send an IRQ if dmc IRQ's are eneabled and the sample has ended.
      dmc_irq = true;
      irq_line++;
    }
  } else if ((dmc->bits_remaining == 0) && (dmc->bytes_remaining == 0)) {
    dmc->silent = true;
  }

  // Use the sample buffer to update the dmc level.
  dmc->bits_remaining = (dmc->bits_remaining > 0) ? dmc->bits_remaining - 1 : 7;
  if (!(dmc->silent)) {
    if ((dmc->sample_buffer & 1) && (dmc->level <= (DMC_LEVEL_MAX - 2))) {
      dmc->level += 2;
    } else if (!(dmc->sample_buffer & 1) && (dmc->level >= 2)) {
      dmc->level -= 2;
    }
  }
  dmc->sample_buffer >>= 1;

  return;
}

/*
 * Sends a sample to the audio system every 40.5 APU cycles.
 *
 * Assumes the APU has been initialized.
 */
static void apu_play_sample(void) {
  // Tracks when the next sample should be played.
  static float sample_clock = 0;

  // Increment the sample clock if a sample is not to be played this cycle.
  if (sample_clock < 37) {
    sample_clock++;
    return;
  }

  // Use NESDEV's formula to linearly approximate the output of the APU.
  float output = (0.00752 * ((float) (pulse_a->output + pulse_b->output)))
               + (0.00851 * ((float) triangle->output))
               + (0.00494 * ((float) noise->output))
               + (0.00335 * ((float) dmc->level));

  // Add the output to the sample buffer.
  audio_add_sample(output);

  // Reset the sample clock.
  sample_clock -= 36.2869375;

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
      // Update the sweep unit control register.
      pulse = (reg_addr == PULSE_A_SWEEP_ADDR) ? pulse_a : pulse_b;
      pulse->sweep = val;
      pulse->sweep_reload = true;
      break;
    case PULSE_A_TIMERL_ADDR:
    case PULSE_B_TIMERL_ADDR:
      // Update the low byte of the timer.
      pulse = (reg_addr == PULSE_A_TIMERL_ADDR) ? pulse_a : pulse_b;
      pulse->timer = (pulse->timer & ~TIMER_LOW_MASK) | val;
      break;
    case PULSE_A_LENGTH_ADDR:
    case PULSE_B_LENGTH_ADDR:
      // Update the pulse length and timer high.
      pulse = (reg_addr == PULSE_A_LENGTH_ADDR) ? pulse_a : pulse_b;
      if (((channel_status & FLAG_PULSE_A_ACTIVE) && (pulse == pulse_a))
         || ((channel_status & FLAG_PULSE_B_ACTIVE) && (pulse == pulse_b))) {
        pulse->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      pulse->timer = (pulse->timer & TIMER_LOW_MASK)
                   | ((((dword_t) val) & TIMER_HIGH_MASK)
                   << TIMER_HIGH_SHIFT);

      // Reset the sequencer position and envelope.
      pulse->pos = 0;
      pulse->env_clock = pulse->control & VOLUME_MASK;
      pulse->env_volume = ENV_DECAY_START;
      break;
    case TRI_CONTROL_ADDR:
      // Update the linear control register.
      triangle->control = val;
      break;
    case TRI_TIMERL_ADDR:
      // Update the low byte of the triangle period.
      triangle->timer = (triangle->timer & ~TIMER_LOW_MASK) | val;
      break;
    case TRI_LENGTH_ADDR:
      // Update the triangle length and timer high.
      if (channel_status & FLAG_TRI_ACTIVE) {
        triangle->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      triangle->timer = (triangle->timer & TIMER_LOW_MASK)
                      | ((((dword_t) val) & TIMER_HIGH_MASK)
                      << TIMER_HIGH_SHIFT);

      // Set the linear counter reload flag, which can be cleared by the
      // control bit.
      triangle->linear_reload = true;
      break;
    case NOISE_CONTROL_ADDR:
      // Update the noise envelope and control register.
      noise->control = val;
      break;
    case NOISE_PERIOD_ADDR:
      // Update the period register and timer.
      noise->period = val;
      noise->timer = noise_periods[noise->period & NOISE_PERIOD_MASK];
      break;
    case NOISE_LENGTH_ADDR:
      // Update the noise length counter.
      if (channel_status & FLAG_NOISE_ACTIVE) {
        noise->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      noise->env_clock = noise->control & VOLUME_MASK;
      noise->env_volume = ENV_DECAY_START;
      break;
    case DMC_CONTROL_ADDR:
      // Update the control bits and rate of the DMC channel.
      dmc->control = val & DMC_CONTROL_MASK;
      dmc->rate = val & DMC_RATE_MASK;
      dmc->clock = 0;
      break;
    case DMC_COUNTER_ADDR:
      // Update the PCM output level of the DMC channel.
      dmc->level = val & DMC_LEVEL_MASK;
      break;
    case DMC_ADDRESS_ADDR:
      // Update the base sample address of the DMC channel.
      dmc->addr = (((dword_t) val) << DMC_ADDR_SHIFT) | DMC_ADDR_BASE;
      dmc->current_addr = dmc->addr;
      break;
    case DMC_LENGTH_ADDR:
      // Update the sample length of the DMC channel.
      dmc->length = (((dword_t) val) << DMC_LENGTH_SHIFT) | DMC_LENGTH_BASE;
      dmc->bytes_remaining = dmc->length;
      break;
    case APU_STATUS_ADDR:
      apu_status_write(val);
      break;
    case FRAME_COUNTER_ADDR:
      frame_control = val;
      // Clear the frame IRQ if the disable flag was set by the write.
      if ((frame_control & FLAG_IRQ_DISABLE) && frame_irq) {
        frame_irq = false;
        irq_line--;
      }

      // Reset the frame timer.
      // TODO: Add delay.
      frame_clock = 0;
      if (frame_control & FLAG_MODE) {
        frame_step = 1;
        apu_run_frame_step();
      }
      frame_step = 0;
      break;
    default:
      // If the address is invalid, nothing happens.
      break;
  }

  return;
}

/*
 * Write to the APU status register, reseting the channels and clearing
 * the DMC IRQ as necessary.
 *
 * Assumes the APU has been initialized.
 */
static void apu_status_write(word_t val) {
  // Store the enable status of each channel.
  channel_status = val;

  // Clear each channel whose bit was not set.
  if (!(val & FLAG_NOISE_ACTIVE)) { noise->length = 0; }
  if (!(val & FLAG_TRI_ACTIVE)) { triangle->length = 0; }
  if (!(val & FLAG_PULSE_B_ACTIVE)) { pulse_b->length = 0; }
  if (!(val & FLAG_PULSE_A_ACTIVE)) { pulse_a->length = 0; }
  if (!(val & FLAG_DMC_ACTIVE)) {
    dmc->bytes_remaining = 0;
  } else if (dmc->bytes_remaining == 0) {
    // If the DMC bit was set, the channel should be reset without changing
    // the contents of the delta buffer; but only when there are no bytes
    // remaining in the sample.
    dmc->current_addr = dmc->addr;
    dmc->bytes_remaining = dmc->length;
  }

  // Clear the DMC interrupt, if it is active.
  if (dmc_irq) {
    dmc_irq = false;
    irq_line--;
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
    if (dmc->bytes_remaining > 0) { status |= FLAG_DMC_ACTIVE; }
    if (noise->length > 0) { status |= FLAG_NOISE_ACTIVE; }
    if (triangle->length > 0) { status |= FLAG_TRI_ACTIVE; }
    if (pulse_b->length > 0) { status |= FLAG_PULSE_B_ACTIVE; }
    if (pulse_a->length > 0) { status |= FLAG_PULSE_A_ACTIVE; }
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
