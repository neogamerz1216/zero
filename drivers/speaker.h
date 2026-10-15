#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdint.h>

void speaker_play(uint32_t frequency_hz);
void speaker_stop(void);
void speaker_beep(uint32_t frequency_hz, uint32_t duration_ms);
void speaker_play_startup_jingle(void);

#endif
