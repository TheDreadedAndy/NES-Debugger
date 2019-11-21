/*
 * This file contains the emulation for the NES's APU.
 * Note that although the APU is clocked at the same rate as the CPU,
 * internally most channels run at half the rate of the CPU.
 *
 * The APU contains two pulse channels, a noise channel driven by a
 * linear feedback shift register, a triangle wave channel, and
 * a delta modulation channel. These channels are mixed non-linearly.
 * This mixing is currently emulated using a large lookup table, but
 * this is subject to change.
 *
 * Note that the only difference between the pulse channels is how
 * their sweep frequencies are subtracted, with one using ones complement
 * and the other using twos complement. Additionally, note
 * that the triangle channel is clocked at the rate of the CPU.
 *
 * The DMC channel can provide the CPU with an IRQ interrupt whenever
 * it finishes a sample. The frame counter can also provide an IRQ.
 * Multiple IRQ's can be issued at the same time, so long as they
 * are from different sources.
 *
 * Finally, it should be noted that the orignal NES has several filters
 * attatched to its audio output. These filters are recreated in the
 * SDL audio interface, and are not found in this file.
 */

#include "./apu.h"

#include <cstdlib>

#include "../util/util.h"
#include "../util/data.h"
#include "../memory/memory.h"
#include "../sdl/audio_player.h"

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

// These constants are used to size the APU mixing tables.
#define PULSE_TABLE_SIZE 31U
#define TRIANGLE_DIM_SHIFT 11U
#define NOISE_DIM_SHIFT 7U
#define TNDMC_SIZE 32768U

/*
 * The rate value in the DMC channel maps to a number of APU cycles to wait
 * between updates, which corresponds to a given frequency. These wait
 * times are stored in this array.
 */
static const size_t dmc_rates[] = { 214, 190, 170, 160, 143, 127, 113, 107,
                                    95, 80, 71, 64, 53, 42, 36, 27 };

/*
 * The period value of the noise channel is determined using a lookup table
 * of APU cycle wait timers. This table is given here.
 */
static const DoubleWord noise_periods[] = { 2, 4, 8, 16, 32, 48, 64, 80, 101,
                                            127, 190, 254, 381, 508, 1017,
                                            2034 };

/*
 * The pulse (square wave) channels have 4 duty cycle settings which change the
 * wave form output. Here these output settings are represented as bytes with
 * each 1 representing a cycle where the wave form is high.
 */
static const DataWord pulse_waves[] = { 0x40U, 0x60U, 0x78U, 0x9FU };

/*
 * The triangle wave channel loops over a 32 step sequence, which is defined
 * here.
 */
static const DataWord triangle_wave[] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,
                                          4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7,
                                          8, 9, 10, 11, 12, 13, 14, 15 };

/*
 * The APU has a length lookup table, which it uses to translate the values
 * written to the channel length counters into actual cycle lengths.
 */
static const DataWord length_table[] = { 10, 254, 20, 2, 40, 4, 80, 6, 160, 8,
                                         60, 10, 14, 12, 26, 14, 12, 16, 24, 18,
                                         48, 20, 96, 22, 192, 24, 72, 26, 16,
                                         28, 32, 30 };

/*
 * Creates an APU object.
 *
 * Assumes the provided memory object and irq line are valid.
 */
Apu::Apu(AudioPlayer *audio, Memory *memory, DataWord *irq_line) {
  // Stores the provided audio device, memory object, and CPU IRQ line.
  audio_ = audio;
  memory_ = memory;
  irq_line_ = irq_line;

  // Allocate the channels of the APU.
  pulse_a_ = new ApuPulse();
  pulse_b_ = new ApuPulse();
  triangle_ = new ApuTriangle();
  noise_ = new ApuNoise();
  dmc_ = new ApuDmc();

  // Load in the mixing tables for the APU channels.
  InitPulseTable();
  InitTndmcTable();

  // On power-up, the noise shifter is seeded with the value 1.
  noise_->shift = 1;
  return;
}

/*
 * Loads the pulse mixing table from the binary into memory.
 */
void Apu::InitPulseTable(void) {
  // Get the address of the pulse table.
  extern const DataWord _binary_bins_pulse_table_bin_start[];

  // Convert the bytes of the table to floats and load them into memory.
  pulse_table_ = new float[PULSE_TABLE_SIZE];
  union { float f; uint32_t u; } temp;
  for (size_t i = 0; i < PULSE_TABLE_SIZE; i++) {
    temp.u = (_binary_bins_pulse_table_bin_start[(4 * i) + 0])
           | (_binary_bins_pulse_table_bin_start[(4 * i) + 1] << 8)
           | (_binary_bins_pulse_table_bin_start[(4 * i) + 2] << 16)
           | (_binary_bins_pulse_table_bin_start[(4 * i) + 3] << 24);
    pulse_table_[i] = temp.f;
  }

  return;
}

/*
 * Loads the triangle/noise/dmc table from the binary into memory.
 */
void Apu::InitTndmcTable(void) {
  // Get the address of the tndmc table.
  extern const DataWord _binary_bins_tndmc_table_bin_start[];

  // Convert the bytes of the table to floats and load them into memory.
  tndmc_table_ = new float[TNDMC_SIZE];
  union { float f; uint32_t u; } temp;
  for (size_t i = 0; i < TNDMC_SIZE; i++) {
    temp.u = (_binary_bins_tndmc_table_bin_start[(4 * i) + 0])
           | (_binary_bins_tndmc_table_bin_start[(4 * i) + 1] << 8)
           | (_binary_bins_tndmc_table_bin_start[(4 * i) + 2] << 16)
           | (_binary_bins_tndmc_table_bin_start[(4 * i) + 3] << 24);
    tndmc_table_[i] = temp.f;
  }

  return;
}

/*
 * Runs a cycle of the APU emulation.
 */
void Apu::RunCycle(void) {
  // Only the triangle wave updates on odd cycles.
  if (!cycle_even_) {
    UpdateTriangle();
    PlaySample();
    cycle_even_ = !cycle_even_;
    return;
  }

  // Update the frame counter. Must be done in this order to prevent
  // issues with frame counter resets.
  IncFrame();
  if (frame_clock_ == 0) { RunFrameStep(); }

  // Update the channels for this cycle.
  UpdatePulse(pulse_a_);
  UpdatePulse(pulse_b_);
  UpdateTriangle();
  UpdateNoise();
  UpdateDmc();

  // Play a sample, if it is time to do so.
  PlaySample();

  // Toggle the cycle evenness.
  cycle_even_ = !cycle_even_;

  return;
}

/*
 * Performs an APU frame step action according to the current step and mode
 * of the frame counter.
 */
void Apu::RunFrameStep(void) {
  // Clock envelopes.
  if (!((frame_step_ == 3) && (frame_control_ & FLAG_MODE))) {
    UpdatePulseEnvelope(pulse_a_);
    UpdatePulseEnvelope(pulse_b_);
    UpdateNoiseEnvelope();
    UpdateTriangleLinear();
  }

  // Clock length counters and pulse sweep.
  if ((frame_step_ == 1) || ((frame_step_ == 3)
                         && !(frame_control_ & FLAG_MODE))
                         || ((frame_step_ == 4)
                         && (frame_control_ & FLAG_MODE))) {
    UpdateLength();
    UpdateSweep(pulse_a_);
    UpdateSweep(pulse_b_);
  }

  // Send the frame IRQ. The IRQ should only be added to the line if it is not
  // already there.
  if ((frame_step_ == 3) && !(frame_control_ & (FLAG_MODE | FLAG_IRQ_DISABLE))
                         && !frame_irq_) {
    frame_irq_ = true;
    (*irq_line_)++;
  }

  return;
}

/*
 * Updates the envelope counter of a pulse channel.
 *
 * Assumes the provided channel is non-null.
 */
void Apu::UpdatePulseEnvelope(ApuPulse *pulse) {
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
 */
void Apu::UpdateNoiseEnvelope(void) {
  // Check if it is time to update the envelope.
  if (noise_->env_clock == 0) {
    // Reset the envelope clock.
    noise_->env_clock = noise_->control & VOLUME_MASK;
    // If the envelope has ended and should be looped, reset the volume.
    if ((noise_->env_volume == 0) && (noise_->control & FLAG_ENV_LOOP)) {
      noise_->env_volume = ENV_DECAY_START;
    } else if (noise_->env_volume > 0) {
      // Otherwise, decay the volume.
      noise_->env_volume--;
    }
  } else {
    noise_->env_clock--;
  }

  return;
}

/*
 * Updates the linear counter of the triangle channel.
 */
void Apu::UpdateTriangleLinear(void) {
  // Reload the linear counter if the flag has been set.
  if (triangle_->linear_reload) {
    triangle_->linear = triangle_->control & LINEAR_MASK;
  } else if (triangle_->linear > 0) {
    // Otherwise, decrement it until it reaches zero.
    triangle_->linear--;
  }

  // Clear the linear counter reload flag if the control bit is clear.
  if (!(triangle_->control & FLAG_LINEAR_CONTROL)) {
    triangle_->linear_reload = false;
  }

  return;
}

/*
 * Updates the length counter of each channel.
 */
void Apu::UpdateLength(void) {
  // Decrement pulse A length counter.
  if ((pulse_a_->length > 0) && !(pulse_a_->control & FLAG_PULSE_HALT)) {
    pulse_a_->length--;
  }

  // Decrement pulse B length counter.
  if ((pulse_b_->length > 0) && !(pulse_b_->control & FLAG_PULSE_HALT)) {
    pulse_b_->length--;
  }

  // Decrement triangle length counter.
  if ((triangle_->length > 0) && !(triangle_->control & FLAG_TRI_HALT)) {
    triangle_->length--;
  }

  // Decrement noise length counter.
  if ((noise_->length > 0) && !(noise_->control & FLAG_NOISE_HALT)) {
    noise_->length--;
  }

  return;
}

/*
 * Updates a pulse channels period if the sweep counter is 0 and the channel
 * is not muted.
 */
void Apu::UpdateSweep(ApuPulse *pulse) {
  // Update the pulses period, if able.
  DoubleWord target_period = GetPulseTarget(pulse);
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
DoubleWord Apu::GetPulseTarget(ApuPulse *pulse) {
  // Use the shift amount to get the period change form the timer,
  // then negate it if necessary.
  DoubleWord period_change = pulse->timer >> (pulse->sweep
                           & PULSE_SWEEP_SHIFT_MASK);
  if (pulse->sweep & PULSE_SWEEP_NEGATE_MASK) {
    // The two pulse channels subtract the period change in different ways.
    period_change = (pulse == pulse_a_) ? ~period_change : -period_change;
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
 */
void Apu::IncFrame(void) {
  frame_clock_++;

  // Reset the clock if a step has been completed.
  if (frame_clock_ > FRAME_STEP_LENGTH) {
    frame_clock_ = 0;
    frame_step_++;

    // Reset the step if a frame has been completed for the given mode.
    if ((frame_step_ >= 5) || ((frame_step_ >= 4)
                           && !(frame_control_ & FLAG_MODE))) {
      frame_step_ = 0;
    }
  }

  return;
}

/*
 * Updates the output of a pulse channel.
 *
 * Assumes the pulse channel is non-null.
 */
void Apu::UpdatePulse(ApuPulse *pulse) {
  // Determine what the pulse channel is outputting audio on this cycle.
  DataWord sequence = (pulse_waves[(pulse->control & PULSE_DUTY_MASK)
                    >> PULSE_DUTY_SHIFT] << pulse->pos) & PULSE_SEQUENCE_MASK;
  DoubleWord target_period = GetPulseTarget(pulse);
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
 */
void Apu::UpdateTriangle(void) {
  // Update the triangle waves output for this cycle. The triangle wave cannot
  // be silenced, it simply stops updating its position. However, the wave can
  // be given a frequency outside the human range of hearing, in which case we
  // force it to zero.
  triangle_->output = (triangle_->timer > 1) ? triangle_wave[triangle_->pos]
                                             : 0U;

  // Update the triangle clock and output wave form using the timer period.
  if (triangle_->clock > 0) {
    triangle_->clock--;
  } else {
    triangle_->clock = triangle_->timer;
    // Determine if the position should be updated.
    if ((triangle_->linear > 0) && (triangle_->length > 0)) {
      triangle_->pos = (triangle_->pos + 1) & 0x1FU;
    }
  }

  return;
}

/*
 * Updates the output of the noise channel.
 */
void Apu::UpdateNoise(void) {
  // Determine what sound should be output this cycle.
  if ((noise_->length > 0) && !(noise_->shift & 0x01U)) {
    noise_->output = (noise_->control & FLAG_CONST_VOL)
                   ? noise_->control & VOLUME_MASK
                   : noise_->env_volume;
  } else {
    noise_->output = 0;
  }

  // Check if it is time to update the noise channel.
  if (noise_->clock > 0) {
    noise_->clock--;
  } else {
    noise_->clock = noise_->timer;
    UpdateNoiseShift();
  }

  return;
}

/*
 * Updates the shift register of the noise channel.
 */
void Apu::UpdateNoiseShift(void) {
  // Calculate the new feedback bit of the noise channel using the noise mode.
  DoubleWord feedback;
  if (noise_->period & FLAG_NOISE_MODE) {
    // In mode 1, the new bit is bit 0 XOR bit 6.
    feedback = (noise_->shift & 0x01U) ^ ((noise_->shift >> 6U) & 0x01U);
  } else {
    // In mode 0, the new bit is bit 0 XOR bit 1.
    feedback = (noise_->shift & 0x01U) ^ ((noise_->shift >> 1U) & 0x01U);
  }

  // Shift the noise shifter and backfill with the feedback bit.
  noise_->shift = (feedback << 14U) | (noise_->shift >> 1U);

  return;
}

/*
 * Updates the DMC channel, filling the buffer and updating the level as
 * necessary. May optionally generate an IRQ when a sample finishes.
 */
void Apu::UpdateDmc(void) {
  // Check if it is time for the DMC channel to update.
  if (dmc_->clock >= dmc_rates[dmc_->rate]) {
    dmc_->clock = 0;
  } else {
    dmc_->clock++;
    return;
  }

  // Refill the sample buffer if it is empty.
  if ((dmc_->bits_remaining == 0) && (dmc_->bytes_remaining > 0)) {
    // TODO: Stall CPU.
    // Load the next word into the sample buffer.
    dmc_->sample_buffer = memory_->Read(dmc_->current_addr);
    dmc_->current_addr = (dmc_->current_addr + 1) | DMC_CURRENT_ADDR_BASE;
    dmc_->bytes_remaining--;
    dmc_->silent = false;

    if ((dmc_->bytes_remaining == 0) && (dmc_->control & FLAG_DMC_LOOP)) {
      dmc_->current_addr = dmc_->addr;
      dmc_->bytes_remaining = dmc_->length;
    } else if ((dmc_->bytes_remaining == 0) && (dmc_->control & FLAG_DMC_IRQ)
                                            && !dmc_irq_) {
      // Send an IRQ if dmc IRQ's are eneabled and the sample has ended.
      dmc_irq_ = true;
      (*irq_line_)++;
    }
  } else if ((dmc_->bits_remaining == 0) && (dmc_->bytes_remaining == 0)) {
    dmc_->silent = true;
  }

  // Use the sample buffer to update the dmc level.
  dmc_->bits_remaining = (dmc_->bits_remaining > 0) ? dmc_->bits_remaining - 1 : 7;
  if (!(dmc_->silent)) {
    if ((dmc_->sample_buffer & 1) && (dmc_->level <= (DMC_LEVEL_MAX - 2))) {
      dmc_->level += 2;
    } else if (!(dmc_->sample_buffer & 1) && (dmc_->level >= 2)) {
      dmc_->level -= 2;
    }
  }
  dmc_->sample_buffer >>= 1;

  return;
}

/*
 * Sends a sample to the audio system every 40.5 APU cycles.
 */
void Apu::PlaySample(void) {
  // Increment the sample clock if a sample is not to be played this cycle.
  if (sample_clock_ < 37) {
    sample_clock_++;
    return;
  }

  // Pull the output of the APU from the mixing tables.
  float pulse_output = pulse_table_[pulse_a_->output + pulse_b_->output];
  float tndmc_output = tndmc_table_[(triangle_->output << TRIANGLE_DIM_SHIFT)
                     | (noise_->output << NOISE_DIM_SHIFT) | dmc_->level];
  float output = pulse_output + tndmc_output;

  // Add the output to the sample buffer.
  audio_->AddSample(output);

  // Reset the sample clock.
  sample_clock_ -= 36.2869375;

  return;
}

/*
 * Writes the given value to a memory mapped APU register.
 * Writes to invalid addresses are ignored.
 */
void Apu::Write(DoubleWord reg_addr, DataWord val) {
  // Determine which register is being accessed.
  ApuPulse *pulse;
  switch(reg_addr) {
    case PULSE_A_CONTROL_ADDR:
    case PULSE_B_CONTROL_ADDR:
      // Update the control and envelope register.
      pulse = (reg_addr == PULSE_A_CONTROL_ADDR) ? pulse_a_ : pulse_b_;
      pulse->control = val;
      break;
    case PULSE_A_SWEEP_ADDR:
    case PULSE_B_SWEEP_ADDR:
      // Update the sweep unit control register.
      pulse = (reg_addr == PULSE_A_SWEEP_ADDR) ? pulse_a_ : pulse_b_;
      pulse->sweep = val;
      pulse->sweep_reload = true;
      break;
    case PULSE_A_TIMERL_ADDR:
    case PULSE_B_TIMERL_ADDR:
      // Update the low byte of the timer.
      pulse = (reg_addr == PULSE_A_TIMERL_ADDR) ? pulse_a_ : pulse_b_;
      pulse->timer = (pulse->timer & ~TIMER_LOW_MASK) | val;
      break;
    case PULSE_A_LENGTH_ADDR:
    case PULSE_B_LENGTH_ADDR:
      // Update the pulse length and timer high.
      pulse = (reg_addr == PULSE_A_LENGTH_ADDR) ? pulse_a_ : pulse_b_;
      if (((channel_status_ & FLAG_PULSE_A_ACTIVE) && (pulse == pulse_a_))
         || ((channel_status_ & FLAG_PULSE_B_ACTIVE) && (pulse == pulse_b_))) {
        pulse->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      pulse->timer = (pulse->timer & TIMER_LOW_MASK)
                   | (((static_cast<DoubleWord>(val)) & TIMER_HIGH_MASK)
                   << TIMER_HIGH_SHIFT);

      // Reset the sequencer position and envelope.
      pulse->pos = 0;
      pulse->env_clock = pulse->control & VOLUME_MASK;
      pulse->env_volume = ENV_DECAY_START;
      break;
    case TRI_CONTROL_ADDR:
      // Update the linear control register.
      triangle_->control = val;
      break;
    case TRI_TIMERL_ADDR:
      // Update the low byte of the triangle period.
      triangle_->timer = (triangle_->timer & ~TIMER_LOW_MASK) | val;
      break;
    case TRI_LENGTH_ADDR:
      // Update the triangle length and timer high.
      if (channel_status_ & FLAG_TRI_ACTIVE) {
        triangle_->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      triangle_->timer = (triangle_->timer & TIMER_LOW_MASK)
                       | (((static_cast<DoubleWord>(val)) & TIMER_HIGH_MASK)
                       << TIMER_HIGH_SHIFT);

      // Set the linear counter reload flag, which can be cleared by the
      // control bit.
      triangle_->linear_reload = true;
      break;
    case NOISE_CONTROL_ADDR:
      // Update the noise envelope and control register.
      noise_->control = val;
      break;
    case NOISE_PERIOD_ADDR:
      // Update the period register and timer.
      noise_->period = val;
      noise_->timer = noise_periods[noise_->period & NOISE_PERIOD_MASK];
      break;
    case NOISE_LENGTH_ADDR:
      // Update the noise length counter.
      if (channel_status_ & FLAG_NOISE_ACTIVE) {
        noise_->length = length_table[(val & LENGTH_MASK) >> LENGTH_SHIFT];
      }
      noise_->env_clock = noise_->control & VOLUME_MASK;
      noise_->env_volume = ENV_DECAY_START;
      break;
    case DMC_CONTROL_ADDR:
      // Update the control bits and rate of the DMC channel.
      dmc_->control = val & DMC_CONTROL_MASK;
      dmc_->rate = val & DMC_RATE_MASK;
      dmc_->clock = 0;
      break;
    case DMC_COUNTER_ADDR:
      // Update the PCM output level of the DMC channel.
      dmc_->level = val & DMC_LEVEL_MASK;
      break;
    case DMC_ADDRESS_ADDR:
      // Update the base sample address of the DMC channel.
      dmc_->addr = ((static_cast<DoubleWord>(val)) << DMC_ADDR_SHIFT)
                 | DMC_ADDR_BASE;
      dmc_->current_addr = dmc_->addr;
      break;
    case DMC_LENGTH_ADDR:
      // Update the sample length of the DMC channel.
      dmc_->length = ((static_cast<DoubleWord>(val)) << DMC_LENGTH_SHIFT)
                   | DMC_LENGTH_BASE;
      dmc_->bytes_remaining = dmc_->length;
      break;
    case APU_STATUS_ADDR:
      StatusWrite(val);
      break;
    case FRAME_COUNTER_ADDR:
      frame_control_ = val;
      // Clear the frame IRQ if the disable flag was set by the write.
      if ((frame_control_ & FLAG_IRQ_DISABLE) && frame_irq_) {
        frame_irq_ = false;
        (*irq_line_)--;
      }

      // Reset the frame timer.
      // TODO: Add delay.
      frame_clock_ = 0;
      if (frame_control_ & FLAG_MODE) {
        frame_step_ = 1;
        RunFrameStep();
      }
      frame_step_ = 0;
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
 */
void Apu::StatusWrite(DataWord val) {
  // Store the enable status of each channel.
  channel_status_ = val;

  // Clear each channel whose bit was not set.
  if (!(val & FLAG_NOISE_ACTIVE)) { noise_->length = 0; }
  if (!(val & FLAG_TRI_ACTIVE)) { triangle_->length = 0; }
  if (!(val & FLAG_PULSE_B_ACTIVE)) { pulse_b_->length = 0; }
  if (!(val & FLAG_PULSE_A_ACTIVE)) { pulse_a_->length = 0; }
  if (!(val & FLAG_DMC_ACTIVE)) {
    dmc_->bytes_remaining = 0;
  } else if (dmc_->bytes_remaining == 0) {
    // If the DMC bit was set, the channel should be reset without changing
    // the contents of the delta buffer; but only when there are no bytes
    // remaining in the sample.
    dmc_->current_addr = dmc_->addr;
    dmc_->bytes_remaining = dmc_->length;
  }

  // Clear the DMC interrupt, if it is active.
  if (dmc_irq_) {
    dmc_irq_ = false;
    (*irq_line_)--;
  }

  return;
}

/*
 * Reads from a memory mapped APU register.
 * All registers, except the status register, are write only and return 0.
 */
DataWord Apu::Read(DoubleWord reg_addr) {
  if (reg_addr == APU_STATUS_ADDR) {
    // Place the status of the APU in a word, and return it.
    DataWord status = 0;
    if (dmc_irq_) { status |= FLAG_DMC_IRQ; }
    if (frame_irq_) {
      // Reading the status register clears the frame IRQ flag.
      status |= FLAG_FRAME_IRQ;
      frame_irq_ = false;
      (*irq_line_)--;
    }
    if (dmc_->bytes_remaining > 0) { status |= FLAG_DMC_ACTIVE; }
    if (noise_->length > 0) { status |= FLAG_NOISE_ACTIVE; }
    if (triangle_->length > 0) { status |= FLAG_TRI_ACTIVE; }
    if (pulse_b_->length > 0) { status |= FLAG_PULSE_B_ACTIVE; }
    if (pulse_a_->length > 0) { status |= FLAG_PULSE_A_ACTIVE; }
    return status;
  } else {
    // Invalid register.
    return 0;
  }
}

/*
 * Frees the APU structures.
 */
Apu::~Apu(void) {
  // Free the channels of the APU.
  delete pulse_a_;
  delete pulse_b_;
  delete triangle_;
  delete noise_;
  delete dmc_;

  // Free the mixing tables.
  delete[] pulse_table_;
  delete[] tndmc_table_;

  return;
}
