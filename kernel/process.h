#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

void process_init(void);
int process_create(void (*entry_point)(void));
void process_schedule(void);

#endif
