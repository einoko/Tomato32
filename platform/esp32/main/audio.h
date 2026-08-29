#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

bool audio_init(void);
void audio_play_bell(void);
bool audio_play_test_sound(uint8_t volume_percent);
bool audio_is_bell_playing(void);
void audio_set_bell_volume(uint8_t percent);

#endif
