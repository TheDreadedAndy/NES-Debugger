/*
 * This file provides an interface between the NES's APU emulation
 * and the SDL audio playback system. Adding this layer both keeps
 * the APU emulation code base cleaner and allows the audio backend
 * to be modified without rewriting the emulation (so long as the
 * interface is preserved).
 *
 * Samples are queued one at a time, with the audio interface applying
 * the NES's low/high pass filters in real time. When the sample buffer
 * is filled, the filtered samples are queued to the audio device and
 * played back to the user. The use of a buffer also allows for the audio
 * samples to played back with correct timing regardless of how fast the
 * emulation is running; however, it also adds a slight delay to playback
 * (about 21 ms).
 *
 * While more than one audio player object can be created at one time,
 * this will cause SDL to use more than one audio device and is, thus,
 * not an intended use of this class.
 */

#include "./audio_player.h"

#include <new>
#include <cstdlib>

#include <SDL2/SDL.h>

#include "../util/util.h"

// The max number of samples the device buffer can hold.
// Must be a power of 2.
#define BUFFER_SIZE 1024U

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
  audio_buffer_ = new float[BUFFER_SIZE]();

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
  // Add the sample to the buffer.
  audio_buffer_[buffer_slot_] = sample;
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
