#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)

enum vga_color {
    COLOR_BLACK = 0, COLOR_BLUE = 1, COLOR_GREEN = 2, COLOR_CYAN = 3,
    COLOR_RED = 4, COLOR_MAGENTA = 5, COLOR_BROWN = 6, COLOR_LIGHT_GREY = 7,
    COLOR_DARK_GREY = 8, COLOR_LIGHT_BLUE = 9, COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN = 11, COLOR_LIGHT_RED = 12, COLOR_LIGHT_MAGENTA = 13,
    COLOR_LIGHT_BROWN = 14, COLOR_WHITE = 15
};

void vga_init(void);
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void vga_clear(uint8_t color);
void vga_set_cursor(int x, int y);
void vga_putc(char c, uint8_t color);
void vga_puts(const char* str, uint8_t color);
void vga_print_at(const char* str, int x, int y, uint8_t color);
void vga_show_splash(void);
void vga_update_boot_step(const char* step_name, int percent);

#endif
