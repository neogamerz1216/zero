#include "rtc.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int get_update_in_progress_flag(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val & 0x0F);
}

void rtc_init(void) {
    outb(CMOS_ADDRESS, 0x8B);
    uint8_t prev = inb(CMOS_DATA);
    outb(CMOS_ADDRESS, 0x8B);
    outb(CMOS_DATA, prev | 0x02);
}

rtc_time_t rtc_get_time(void) {
    rtc_time_t t;

    while (get_update_in_progress_flag());

    t.second = get_rtc_register(0x00);
    t.minute = get_rtc_register(0x02);
    t.hour   = get_rtc_register(0x04);
    t.day    = get_rtc_register(0x07);
    t.month  = get_rtc_register(0x08);
    uint8_t year_val = get_rtc_register(0x09);

    uint8_t register_b = get_rtc_register(0x0B);

    if (!(register_b & 0x04)) {
        t.second = bcd_to_bin(t.second);
        t.minute = bcd_to_bin(t.minute);
        t.hour   = bcd_to_bin(t.hour);
        t.day    = bcd_to_bin(t.day);
        t.month  = bcd_to_bin(t.month);
        year_val = bcd_to_bin(year_val);
    }

    t.year = 2000 + year_val;
    return t;
}
