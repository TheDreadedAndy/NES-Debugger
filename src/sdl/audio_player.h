#ifndef _NES_AUDIO
#define _NES_AUDIO

#include <cstdlib>

#include <SDL2/SDL.h>

/*
 * Allows samples to be sent to an SDL audio device and played back to
 * the user. Filters any output sound as the NES would.
 *
 * Used to play samples created by the APU during the emulation.
 */
class AudioPlayer {
  private:
    // Sample are added to a buffer, which is queued to the device when
    // it becomes full.
    float *audio_buffer_;
    size_t buffer_slot_ = 0;

    // Audio samples are sent to this device, which is picked
    // during construction.
    SDL_AudioDeviceID audio_device_;

    // Allocates the audio buffer and initializes the audio filters.
    AudioPlayer(SDL_AudioDeviceID device);

  public:
    // Attempts to open an audio device and create an audio player. Returns
    // NULL on failure.
    static AudioPlayer *Create(void);

    // Adds a sample to the sample buffer.
    void AddSample(float sample);

    // Closes the audio device and frees the buffer.
    ~AudioPlayer(void);
};

#endif
