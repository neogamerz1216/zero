#include "pic.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void pic_remap(void) {
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();

    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();

    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();

    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();

    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
