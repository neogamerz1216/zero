#include "zedit.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../fs/zerofs.h"

#define MAX_BUFFER 2048

void zedit_open(volume_t* vol, const char* filename) {
    static char text_buf[MAX_BUFFER];
    for (int i = 0; i < MAX_BUFFER; i++) text_buf[i] = 0;

    int len = zerofs_read(vol, filename, text_buf, MAX_BUFFER);
    if (len < 0) len = 0;

    vga_clear(COLOR_BLUE);

    vga_print_at(" ZeroOS Text Editor (zedit) | File: ", 0, 0, COLOR_BLACK | (COLOR_LIGHT_GREY << 4));
    vga_print_at(filename, 36, 0, COLOR_BLACK | (COLOR_LIGHT_GREY << 4));
    vga_print_at(" [ESC] Save & Exit ", 60, 0, COLOR_RED | (COLOR_LIGHT_GREY << 4));

    vga_set_cursor(0, 2);
    for (int i = 0; i < len; i++) {
        vga_putc(text_buf[i], COLOR_WHITE);
    }

    while (1) {
        char c = keyboard_getchar();

        if (c == 27) {
            zerofs_write(vol, filename, text_buf, len);
            break;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                text_buf[len] = '\0';
                vga_putc('\b', COLOR_WHITE);
            }
        } else if (c == '\n') {
            if (len < MAX_BUFFER - 1) {
                text_buf[len++] = '\n';
                vga_putc('\n', COLOR_WHITE);
            }
        } else if (c >= 32 && c <= 126) {
            if (len < MAX_BUFFER - 1) {
                text_buf[len++] = c;
                vga_putc(c, COLOR_WHITE);
            }
        }
    }

    vga_clear(COLOR_BLACK);
}
