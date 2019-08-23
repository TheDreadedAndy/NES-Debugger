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
#define BUFFER_SIZE 1024U

// The NES has 3 filters, which must be applied to the samples in the buffer
// to correctly output audio. These smoothing factors are calculated using a
// sampling rate of 48000.
#define HPF1_SMOOTH 0.988356
#define HPF2_SMOOTH 0.945541
#define LPF_SMOOTH 0.646967

/*
 * Audio samples are stored in a buffer, which is queued when
 * audio_play_frame() is called.
 */
static float *audio_buffer = NULL;
static size_t buffer_slot = 0;

/*
 * Audio samples will be played to this device, which is obtained from
 * SDL_OpenAudioDevice().
 */
static SDL_AudioDeviceID audio_device = 0;

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
  want.samples = BUFFER_SIZE;
  want.callback = NULL;

  // Init the device.
  audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device == 0) {
    fprintf(stderr, "Failed to open audio device\n");
    return false;
  }

  // Prepare the audio buffer.
  audio_buffer = xcalloc(BUFFER_SIZE, sizeof(float));

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
  if (audio_device == 0) { return; }

  // These variables are used to filter the buffer.
  static float last_normal_sample = 0;
  static float last_hpf1_sample = 0;
  static float last_hpf2_sample = 0;
  static float last_lpf_sample = 0;

  // Apply the first high pass filter (90Hz).
  float sample_temp = HPF1_SMOOTH * (last_hpf1_sample + sample
                                  - last_normal_sample);
  last_normal_sample = sample;

  // Apply the second high pass filter (440Hz).
  sample = HPF2_SMOOTH * (last_hpf2_sample + sample_temp
                       - last_hpf1_sample);
  last_hpf1_sample = sample_temp;
  last_hpf2_sample = sample;

  // Apply the low pass filter (14KHz).
  sample_temp = (LPF_SMOOTH * sample)
              + ((1.0 - LPF_SMOOTH) * last_lpf_sample);
  last_lpf_sample = sample_temp;

  // Finally, we add the sample to the buffer.
  audio_buffer[buffer_slot] = sample_temp;
  buffer_slot++;

  // If the buffer has filled, we queue it to the device.
  if (buffer_slot >= BUFFER_SIZE) {
    buffer_slot = 0;
    SDL_QueueAudio(audio_device, audio_buffer, sizeof(float) * BUFFER_SIZE);
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
