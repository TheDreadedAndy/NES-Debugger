/*
 * TODO
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "./audio.h"
#include "../util/util.h"

// The max number of samples the buffer can hold.
#define BUFFER_SIZE 735

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
  want.freq = 44100;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  // Samples are added by audio_play_frame().
  want.callback = NULL;

  // The device is initialized.
  audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device == 0) {
    fprintf(stderr, "Failed to open audio device\n");
    return false;
  }

  // The audio buffer is allocated.
  audio_buffer = xcalloc(sizeof(float), BUFFER_SIZE);

  // Unpause the device and return success.
  SDL_PauseAudioDevice(audio_device, 0);
  return true;
}

/*
 * Adds a sample to the audio buffer.
 *
 * Does nothing if the buffer is full or if audio has not been initialized.
 */
void audio_add_sample(float sample) {
  // If the buffer is full, we do nothing.
  // The buffer is emptied when audio_play_frame() is called.
  if ((audio_device == 0) || (buffer_slot >= BUFFER_SIZE)) { return; }

  // Otherwise, we add the sample to the buffer.
  audio_buffer[buffer_slot] = sample;
  buffer_slot++;
  return;
}

/*
 * Empties the audio buffer to the device, then resets the buffer.
 *
 * Does nothing if audio has not been initialized.
 */
void audio_play_frame(void) {
  // If audio has not been initialized, we return.
  if (audio_device == 0) { return; }

  // Otherwise, queue the buffered samples and clear the buffer.
  if (SDL_QueueAudio(audio_device, (void*) audio_buffer,
      sizeof(float) * buffer_slot) < 0) {
    fprintf(stderr, "Failed to play audio: %s\n", SDL_GetError());
  }
  buffer_slot = 0;
  memset(audio_buffer, 0, sizeof(float) * BUFFER_SIZE);
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
