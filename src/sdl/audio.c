/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "./audio.h"
#include "../util/util.h"

// The max number of samples the device buffer can hold.
// Must be a power of 2.
#define DEVICE_BUFFER_SIZE 1024U

// The device may not request samples as fast as the emulation provides
// them, so the emulation must have a larger audio buffer.
#define EMU_BUFFER_SIZE (DEVICE_BUFFER_SIZE << 2U)

// The NES has 3 filters, which must be applied to the samples in the buffer
// to correctly output audio. These smoothing factors are calculated using a
// sampling rate of 48000.
#define HPF1_SMOOTH 0.988356
#define HPF2_SMOOTH 0.945541
#define LPF_SMOOTH 0.646967

/*
 * The emulation audio buffer is implemented as a ring buffer to
 * increase speed.
 */
typedef struct audio_buffer {
  float *buffer;
  size_t base;
  size_t size;
} buffer_t;

/*
 * Audio samples are stored in a buffer, which is queued when
 * audio_play_frame() is called.
 */
static buffer_t *audio_buffer = NULL;
static size_t buffer_slot = 0;

/*
 * This buffer is used for temperary storage while applying the filters.
 */
static float *filter_buffer = NULL;

/*
 * Audio samples will be played to this device, which is obtained from
 * SDL_OpenAudioDevice().
 */
static SDL_AudioDeviceID audio_device = 0;

/* Helper functions */
void audio_callback(void *userdata, uint8_t *stream, int len);
float audio_buffer_read(size_t index);
void audio_buffer_write(size_t index, float val);
void audio_buffer_set_base(size_t offset);

/*
 * Initializes the audio system by allocating the buffer and opening the device.
 *
 * Assumes SDL has been initialized.
 */
bool audio_init(void) {
  // The specs of the audio system this file expects are layed out here.
  SDL_AudioSpec want, have;
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = DEVICE_BUFFER_SIZE;
  want.callback = &audio_callback;

  // Init the device.
  audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device == 0) {
    fprintf(stderr, "Failed to open audio device\n");
    return false;
  }

  // Prepare the audio buffer.
  audio_buffer = xcalloc(1, sizeof(buffer_t));
  audio_buffer->buffer = xcalloc(EMU_BUFFER_SIZE, sizeof(float));
  audio_buffer->size = EMU_BUFFER_SIZE;
  filter_buffer = xcalloc(DEVICE_BUFFER_SIZE, sizeof(float));

  // Unpause the device and return success.
  SDL_PauseAudioDevice(audio_device, 0);
  return true;
}

/*
 * Adds a sample to the audio buffer.
 *
 * Does nothing if the buffer is full or audio has not been initialized.
 */
void audio_add_sample(float sample) {
  // If audio is not ready, we do nothing.
  if ((audio_device == 0) || (buffer_slot >= EMU_BUFFER_SIZE)) { return; }

  // Otherwise, we add the sample to the buffer.
  // The buffer must be protected from audio callbacks while this is done.
  SDL_LockAudioDevice(audio_device);
  audio_buffer_write(buffer_slot, sample);
  buffer_slot++;
  SDL_UnlockAudioDevice(audio_device);

  return;
}

/*
 * Empties the audio buffer to the device, then resets the emulation buffer.
 * If the emulation audio buffer cannot fill the device audio buffer,
 * the device buffer is topped off with the last sample of the emulation
 * buffer.
 */
void audio_callback(void *userdata, uint8_t *stream, int len) {
  // Used to make the filter continuous.
  static float last_normal_sample = 0;
  static float last_hpf1_sample = 0;
  static float last_hpf2_sample = 0;

  // Get the device buffer and its size from the inputs.
  size_t device_buffer_size = len / sizeof(float);
  float *device_buffer = (float*) stream;
  (void)userdata;

  // Fill in the device buffer using the emulation audio buffer, while also
  // applying a 90Hz high pass filter.
  device_buffer[0] = (buffer_slot) ? audio_buffer_read(0) : 0.0;
  device_buffer[0] = HPF1_SMOOTH * (last_hpf1_sample + device_buffer[0]
                                 - last_normal_sample);
  size_t i = 1;
  size_t stop = (buffer_slot < device_buffer_size) ? buffer_slot
                                                   : device_buffer_size;
  last_normal_sample = audio_buffer_read(stop - 1);
  for (; i < stop; i++) {
    device_buffer[i] = HPF1_SMOOTH * (device_buffer[i - 1]
                     + audio_buffer_read(i) - audio_buffer_read(i - 1));
  }

  // Copy the device buffer to the filter buffer, applying a 440Hz high pass
  // filter.
  filter_buffer[0] = HPF2_SMOOTH * (last_hpf2_sample + device_buffer[0]
                                 - last_hpf1_sample);
  last_hpf1_sample = device_buffer[stop - 1];
  for (i = 1; i < stop; i++) {
    filter_buffer[i] = HPF2_SMOOTH * (filter_buffer[i - 1] + device_buffer[i]
                                   - device_buffer[i - 1]);
  }

  // Finally, copy the filter buffer back to the device buffer, applying a 14KHz
  // low pass filter.
  device_buffer[0] = (LPF_SMOOTH * filter_buffer[0])
                   + ((1.0 - LPF_SMOOTH) * last_hpf2_sample);
  last_hpf2_sample = device_buffer[stop - 1];
  for (i = 1; i < stop; i++) {
    device_buffer[i] = (LPF_SMOOTH * filter_buffer[i])
                     + ((1.0 - LPF_SMOOTH) * device_buffer[i - 1]);
  }

  // Fill in any unused space.
  for (; i < device_buffer_size; i++) {
    device_buffer[i] = device_buffer[i - 1];
  }

  // Update the emulation audio buffer.
  if (device_buffer_size < buffer_slot) {
    buffer_slot = buffer_slot - device_buffer_size;
    audio_buffer_set_base(device_buffer_size);
  } else {
    buffer_slot = 0;
  }

  return;
}

/*
 * Cleans up the audio system.
 *
 * Does nothing if audio has not been initialized.
 */
void audio_close(void) {
  // If audio has not been initialized, we do nothing.
  if (audio_device == 0) { return; }

  // Close the open device and free the buffer.
  SDL_CloseAudioDevice(audio_device);
  free(audio_buffer);

  return;
}

/*
 * Reads a sample from the audio buffer.
 *
 * Assumes the audio buffer is non-null.
 */
float audio_buffer_read(size_t index) {
  return audio_buffer->buffer[(audio_buffer->base + index)
                                    % audio_buffer->size];
}

/*
 * Writes a sample to the audio buffer.
 *
 * Assumes the audio buffer is non-null.
 */
void audio_buffer_write(size_t index, float val) {
  audio_buffer->buffer[(audio_buffer->base + index) % audio_buffer->size] = val;
  return;
}

/*
 * Adds the given offset to the audio buffer base.
 *
 * Assumes the audio buffer has been initialized.
 */
void audio_buffer_set_base(size_t offset) {
  audio_buffer->base = (audio_buffer->base + offset) % audio_buffer->size;
  return;
}
