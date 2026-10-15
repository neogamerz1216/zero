#include "keyboard.h"
#include "../kernel/pic.h"
#include <stdint.h>

#define KBD_BUFFER_SIZE 256
static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int buf_head = 0;
static volatile int buf_tail = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static const char kbd_us[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

void keyboard_init(void) {
    buf_head = 0;
    buf_tail = 0;

    while (inb(0x64) & 1) {
        inb(0x60);
    }
    outb(0x64, 0xAE);
}

void keyboard_handler(void) {
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t scancode = inb(0x60);
        if (!(scancode & 0x80)) {
            if (scancode < 128) {
                char c = kbd_us[scancode];
                if (c != 0) {
                    int next = (buf_head + 1) % KBD_BUFFER_SIZE;
                    if (next != buf_tail) {
                        kbd_buffer[buf_head] = c;
                        buf_head = next;
                    }
                }
            }
        }
    }
    pic_send_eoi(1);
}

char keyboard_getchar(void) {
    while (buf_head == buf_tail) {
        __asm__ volatile ("sti; hlt");
    }

    char c = kbd_buffer[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

void keyboard_drain(void) {
    __asm__ volatile ("cli");
    buf_tail = buf_head;
    __asm__ volatile ("sti");
}
