#include <stdlib.h>
#include <SDL2/SDL.h>

#ifndef _NES_AUDIO
#define _NES_AUDIO

// Initializes audio.
bool audio_init(void);

// Adds a sample to the sample buffer.
void audio_add_sample(float sample);

// Closes the audio device and frees the buffer.
void audio_close(void);

#endif