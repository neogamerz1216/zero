#include "pic.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

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

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = irq < 8 ? 0x21 : 0xA1;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8);
    uint8_t mask = inb(port);
    outb(port, (uint8_t)(mask & (uint8_t)~(1U << bit)));
    if (irq >= 8) {
        mask = inb(0x21);
        outb(0x21, (uint8_t)(mask & (uint8_t)~(1U << 2)));
    }
}
