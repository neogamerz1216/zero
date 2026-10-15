#include "vga.h"

static int cursor_x = 0;
static int cursor_y = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void vga_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_clear(uint8_t bg) {
    uint16_t blank = (uint16_t)' ' | ((uint16_t)(bg << 4) << 8);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_set_cursor(0, 0);
}

void vga_init(void) {
    vga_clear(COLOR_BLACK);
    vga_enable_cursor(14, 15);
    vga_set_cursor(0, 0);
}

static void vga_scroll(void) {
    uint16_t blank = (uint16_t)' ' | ((uint16_t)(COLOR_BLACK << 4) << 8);
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];
    }
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA_MEMORY[i] = blank;
    }
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)' ' | ((uint16_t)color << 8);
        }
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)c | ((uint16_t)color << 8);
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    vga_set_cursor(cursor_x, cursor_y);
}

void vga_puts(const char* str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i], color);
    }
}

void vga_print_at(const char* str, int x, int y, uint8_t color) {
    int cur = y * VGA_WIDTH + x;
    for (int i = 0; str[i] != '\0'; i++) {
        VGA_MEMORY[cur++] = (uint16_t)str[i] | ((uint16_t)color << 8);
    }
}

void vga_show_splash(void) {
    vga_clear(COLOR_BLACK);
    vga_print_at("  ______                   ____   _____ ", 18, 4, COLOR_LIGHT_CYAN);
    vga_print_at(" |___  /                  / __ \\ / ____|", 18, 5, COLOR_LIGHT_CYAN);
    vga_print_at("    / /  ___ _ __ ___    | |  | | (___  ", 18, 6, COLOR_LIGHT_CYAN);
    vga_print_at("   / /  / _ \\ '__/ _ \\   | |  | |\\___ \\ ", 18, 7, COLOR_LIGHT_CYAN);
    vga_print_at("  / /__|  __/ | | (_) |  | |__| |____) |", 18, 8, COLOR_CYAN);
    vga_print_at(" /_____|\\___|_|  \\___/    \\____/|_____/ ", 18, 9, COLOR_CYAN);

    vga_print_at(">> Enhanced By ZeroOS <<", 27, 12, COLOR_LIGHT_GREEN);
    vga_print_at("Independent 64-Bit Operating System Core", 19, 14, COLOR_WHITE);
    vga_print_at("[                                                  ]", 14, 17, COLOR_WHITE);
}

void vga_update_boot_step(const char* step_name, int percent) {
    for (int i = 0; i < 60; i++) {
        VGA_MEMORY[19 * VGA_WIDTH + 10 + i] = (uint16_t)' ' | ((uint16_t)COLOR_BLACK << 8);
    }
    vga_print_at(step_name, 14, 19, COLOR_LIGHT_GREEN);

    int filled = (percent * 50) / 100;
    for (int i = 0; i < filled; i++) {
        VGA_MEMORY[17 * VGA_WIDTH + 15 + i] = (uint16_t)'=' | ((uint16_t)COLOR_LIGHT_CYAN << 8);
    }
}
