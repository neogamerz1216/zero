#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
void mouse_handler(void);
void mouse_get_state(int* x, int* y, uint8_t* buttons);

#endif