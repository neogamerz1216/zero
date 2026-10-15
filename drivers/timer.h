#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t hz);
void timer_handler(void);
void timer_sleep_ms(uint32_t ms);

#endif
