#ifndef ZEROOS_SYSCALL_H
#define ZEROOS_SYSCALL_H

#include <stdint.h>

enum {
    SYS_PRINT = 1,
    SYS_PUTC = 2,
    SYS_CLEAR = 3,
    SYS_GETCHAR = 4,
    SYS_SLEEP = 5,
    SYS_BEEP = 6,
    SYS_SETCURSOR = 7,
    SYS_BRK = 8,
    SYS_EXIT = 9
};

long syscall_dispatch(long num, long a, long b, long c);
void syscall_init(void);

#endif
