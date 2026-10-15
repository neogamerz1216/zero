#include "syscall.h"
#include "user.h"
#include "mm.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/speaker.h"

#define SYSCALL_MAX_STRING 2048

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

extern void syscall_entry(void);

void syscall_init(void) {
    uint64_t efer = rdmsr(0xC0000080);
    efer |= 1ULL | (1ULL << 11);
    wrmsr(0xC0000080, efer);
    wrmsr(0xC0000081, (0x08ULL << 32) | (0x18ULL << 48));
    wrmsr(0xC0000082, (uint64_t)syscall_entry);
    wrmsr(0xC0000084, 0x200ULL);
}

static long syscall_copy_user_string(uint64_t src, char* dst, size_t max) {
    size_t len = 0;
    while (len < max) {
        unsigned char ch;
        if (user_copy_from_user(&ch, src + len, 1) != 0) {
            return -1;
        }
        dst[len] = (char)ch;
        if (ch == 0) {
            return (long)len;
        }
        len++;
    }
    return -1;
}

long syscall_dispatch(long num, long a, long b, long c) {
    (void)b;
    switch (num) {
    case SYS_PRINT: {
        if (a == 0 || c < 0 || c > 15) {
            return -1;
        }
        char* kbuf = (char*)kmalloc(SYSCALL_MAX_STRING + 1);
        if (!kbuf) {
            return -1;
        }
        long len = syscall_copy_user_string((uint64_t)a, kbuf, SYSCALL_MAX_STRING);
        if (len < 0) {
            kfree(kbuf);
            return -1;
        }
        vga_puts(kbuf, (uint8_t)c);
        kfree(kbuf);
        return len;
    }
    case SYS_PUTC: {
        if (c < 0 || c > 15) {
            return -1;
        }
        if (a != '\n' && (a < 32 || a > 126)) {
            return -1;
        }
        vga_putc((char)a, (uint8_t)c);
        return 0;
    }
    case SYS_CLEAR: {
        if (a < 0 || a > 15) {
            return -1;
        }
        vga_clear((uint8_t)a);
        return 0;
    }
    case SYS_GETCHAR:
        return (long)keyboard_getchar();
    case SYS_SLEEP: {
        if (a < 0 || a > 5000) {
            return -1;
        }
        timer_sleep_ms((uint32_t)a);
        return 0;
    }
    case SYS_BEEP: {
        if (a < 30 || a > 20000 || b < 0 || b > 2000) {
            return -1;
        }
        speaker_beep((uint32_t)a, (uint32_t)b);
        return 0;
    }
    case SYS_SETCURSOR: {
        if (a < 0 || a >= 80 || b < 0 || b >= 25) {
            return -1;
        }
        vga_set_cursor((int)a, (int)b);
        return 0;
    }
    case SYS_BRK:
        return user_process_set_brk(a);
    case SYS_EXIT:
        user_process_exit((int)a);
        return 0;
    default:
        return -1;
    }
}
