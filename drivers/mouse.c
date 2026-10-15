#include "mouse.h"
#include "../kernel/pic.h"

#define MOUSE_WIDTH 80
#define MOUSE_HEIGHT 25

static volatile int mouse_x = MOUSE_WIDTH / 2;
static volatile int mouse_y = MOUSE_HEIGHT / 2;
static volatile uint8_t mouse_buttons;
static uint8_t packet[3];
static uint8_t packet_index;

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static int wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(inb(0x64) & 0x02)) return 1;
    }
    return 0;
}

static int wait_output_full(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(0x64) & 0x01) return 1;
    }
    return 0;
}

static int mouse_write(uint8_t value) {
    if (!wait_input_clear()) return 0;
    outb(0x64, 0xD4);
    if (!wait_input_clear()) return 0;
    outb(0x60, value);
    return 1;
}

void mouse_init(void) {
    mouse_x = MOUSE_WIDTH / 2;
    mouse_y = MOUSE_HEIGHT / 2;
    mouse_buttons = 0;
    packet_index = 0;

    if (!wait_input_clear()) return;
    outb(0x64, 0xA8);
    if (!wait_input_clear()) return;
    outb(0x64, 0x20);
    if (!wait_output_full()) return;
    uint8_t config = inb(0x60);
    config |= 0x02;
    config &= (uint8_t)~0x20;
    if (!wait_input_clear()) return;
    outb(0x64, 0x60);
    if (!wait_input_clear()) return;
    outb(0x60, config);

    if (!mouse_write(0xF4) || !wait_output_full()) return;
    if (inb(0x60) != 0xFA) return;
    pic_unmask_irq(12);
}

void mouse_handler(void) {
    uint8_t status = inb(0x64);
    if ((status & 0x21) == 0x21) {
        uint8_t value = inb(0x60);
        if (packet_index == 0 && !(value & 0x08)) {
            pic_send_eoi(12);
            return;
        }
        packet[packet_index++] = value;
        if (packet_index == 3) {
            packet_index = 0;
            if (!(packet[0] & 0xC0)) {
                int dx = (int)(int8_t)packet[1];
                int dy = (int)(int8_t)packet[2];
                mouse_x += dx;
                mouse_y -= dy;
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= MOUSE_WIDTH) mouse_x = MOUSE_WIDTH - 1;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= MOUSE_HEIGHT) mouse_y = MOUSE_HEIGHT - 1;
                mouse_buttons = packet[0] & 0x07;
            }
        }
    }
    pic_send_eoi(12);
}

void mouse_get_state(int* x, int* y, uint8_t* buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}