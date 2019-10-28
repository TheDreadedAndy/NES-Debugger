/*
 * TODO
 */

#include "./audio_player.h"

#include <new>
#include <cstdlib>

#include <SDL2/SDL.h>

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
 * Creates an AudioPlayer by opening an audio device and using it to
 * construct an AudioPlayer.
 *
 * On success, returns an AudioPlayer. If an audio device cannot be opened,
 * returns NULL.
 */
AudioPlayer *AudioPlayer::Create(void) {
  // The specs of the audio system this file expects are layed out here.
  SDL_AudioSpec want, have;
  want.freq = 48000;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = BUFFER_SIZE;
  want.callback = NULL;

  // Init the device.
  SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (device == 0) {
    fprintf(stderr, "Failed to open audio device\n");
    return NULL;
  }

  // Since the device was opened successfully, we construct an AudioPlayer
  // and return it to the caller.
  return new AudioPlayer(device);
}


/*
 * Initializes the audio system by allocating the buffer and preparing
 * the filters.
 */
AudioPlayer::AudioPlayer(SDL_AudioDeviceID device) {
  // Prepare the audio buffer.
  audio_buffer_ = new float[BUFFER_SIZE];

  // Initialize the filters.
  last_normal_sample_ = 0;
  last_hpf1_sample_ = 0;
  last_hpf2_sample_ = 0;
  last_lpf_sample_ = 0;

  // Store and unpause the device.
  audio_device_ = device;
  SDL_PauseAudioDevice(audio_device_, 0);

  return;
}

/*
 * Adds a sample to the audio buffer.
 * If this sample fills the buffer, the buffer is queued to the audio device.
 */
void AudioPlayer::AddSample(float sample) {
  // Apply the first high pass filter (90Hz).
  float sample_temp = HPF1_SMOOTH * (last_hpf1_sample_ + sample
                                  - last_normal_sample_);
  last_normal_sample_ = sample;

  // Apply the second high pass filter (440Hz).
  sample = HPF2_SMOOTH * (last_hpf2_sample_ + sample_temp
                       - last_hpf1_sample_);
  last_hpf1_sample_ = sample_temp;
  last_hpf2_sample_ = sample;

  // Apply the low pass filter (14KHz).
  sample_temp = (LPF_SMOOTH * sample)
              + ((1.0 - LPF_SMOOTH) * last_lpf_sample_);
  last_lpf_sample_ = sample_temp;

  // Finally, we add the sample to the buffer.
  audio_buffer_[buffer_slot_] = sample_temp;
  buffer_slot_++;

  // If the buffer has filled, we queue it to the device.
  if (buffer_slot_ >= BUFFER_SIZE) {
    buffer_slot_ = 0;
    SDL_QueueAudio(audio_device_, audio_buffer_, sizeof(float) * BUFFER_SIZE);
  }

  return;
}

/*
 * Closes the audio device and frees the audio buffer.
 */
AudioPlayer::~AudioPlayer(void) {
  // Close the open device and free the buffer.
  SDL_CloseAudioDevice(audio_device_);
  delete[] audio_buffer_;

  return;
}
