#include "zap.h"
#include "../kernel/syscall.h"
#include <stdint.h>

static inline long zsys(long num, long a, long b, long c) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory"
    );
    return ret;
}

__attribute__((section(".user_text")))
void user_api_print(const char* str, uint8_t color) {
    if (!str) {
        return;
    }
    zsys(SYS_PRINT, (long)str, 0, (long)color);
}

__attribute__((section(".user_text")))
void user_api_putc(char c, uint8_t color) {
    zsys(SYS_PUTC, (long)c, 0, (long)color);
}

__attribute__((section(".user_text")))
void user_api_clear(uint8_t bg) {
    zsys(SYS_CLEAR, (long)bg, 0, 0);
}

__attribute__((section(".user_text")))
char user_api_getchar(void) {
    return (char)zsys(SYS_GETCHAR, 0, 0, 0);
}

__attribute__((section(".user_text")))
void user_api_sleep(uint32_t ms) {
    zsys(SYS_SLEEP, (long)ms, 0, 0);
}

__attribute__((section(".user_text")))
void user_api_beep(uint32_t freq, uint32_t ms) {
    zsys(SYS_BEEP, (long)freq, (long)ms, 0);
}

__attribute__((section(".user_text")))
void user_api_set_cursor(int x, int y) {
    zsys(SYS_SETCURSOR, (long)x, (long)y, 0);
}

__attribute__((section(".user_text")))
void user_app_trampoline(const zap_api_t* api, zap_main_func app) {
    int status = 0;
    if (app) {
        status = app(api);
    }
    zsys(SYS_EXIT, (long)status, 0, 0);
    while (1) {
    }
}
