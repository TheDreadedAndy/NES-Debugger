/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "./audio.h"
#include "../util/util.h"

// The max number of samples the buffer can hold.
// Must be a power of 2.
#define DEVICE_BUFFER_SIZE 1024U
#define EMU_BUFFER_SIZE (DEVICE_BUFFER_SIZE << 1U)

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
  want.freq = 44100;
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
  // Get the device buffer and its size from the inputs.
  size_t device_buffer_size = len / sizeof(float);
  float *device_buffer = (float*) stream;
  (void)userdata;

  // Fill in the device buffer using the emulation audio buffer.
  for (size_t i = 0; i < device_buffer_size; i++) {
    float sample = (i < buffer_slot) ? audio_buffer_read(i)
                                     : audio_buffer_read(buffer_slot - 1);
    device_buffer[i] = sample;
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
