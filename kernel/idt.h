#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_init(void);
uint64_t exception_dispatch(uint64_t num, uint64_t err, uint64_t* frame);

#endif
