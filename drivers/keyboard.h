#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_getchar(void);
int keyboard_try_getchar(char* value);
void keyboard_handler(void);
void keyboard_drain(void);

#endif
