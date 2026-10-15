#include "timer.h"
#include "../kernel/process.h"
#include "../kernel/pic.h"
#include "../kernel/user.h"

static volatile uint64_t timer_ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(uint32_t hz) {
    timer_ticks = 0;
    uint32_t divisor = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler(void) {
    timer_ticks++;
    pic_send_eoi(0);

    if (!user_process_active() && timer_ticks % 10 == 0) {
        process_schedule();
    }
}

void timer_sleep_ms(uint32_t ms) {
    uint64_t target_ticks = timer_ticks + (ms / 10);
    while (timer_ticks < target_ticks) {
        __asm__ volatile ("sti; hlt");
    }
}
