#include "speaker.h"
#include "timer.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void speaker_play(uint32_t frequency_hz) {
    if (frequency_hz == 0) return;
    uint32_t div = 1193180 / frequency_hz;

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void speaker_stop(void) {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void speaker_beep(uint32_t frequency_hz, uint32_t duration_ms) {
    speaker_play(frequency_hz);
    timer_sleep_ms(duration_ms);
    speaker_stop();
}

void speaker_play_startup_jingle(void) {
    speaker_beep(523, 100);
    timer_sleep_ms(30);
    speaker_beep(659, 100);
    timer_sleep_ms(30);
    speaker_beep(784, 180);
}
