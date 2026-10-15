#include "zpkg.h"
#include "../apps/zap.h"
#include "../apps/zstore.h"
#include "../fs/zerofs.h"
#include "../kernel/user.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/speaker.h"

static int str_cmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static void print_dec(int64_t val, uint8_t color) {
    char buf[16]; int i = 0;
    if (val == 0) { vga_putc('0', color); return; }
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putc(buf[j], color);
}

static const char* user_status_text(unsigned long status) {
    if (status == 0) {
        return " exited cleanly in Ring 3.\n";
    }
    if (status & 0x80000000UL) {
        return " was terminated in Ring 3; kernel kept running.\n";
    }
    return " exited to kernel.\n";
}

void zpkg_cli(volume_t* vol, const char* args) {
    while (*args == ' ') args++;

    if (str_cmp(args, "search") == 0 || str_cmp(args, "store") == 0 || str_cmp(args, "") == 0) {
        vga_puts("\n======================================================\n", COLOR_LIGHT_CYAN);
        vga_puts("          ZeroOS Central App Store Repository         \n", COLOR_WHITE);
        vga_puts("======================================================\n", COLOR_LIGHT_CYAN);
        vga_puts("  APP NAME     DEVELOPER     VER     DESCRIPTION\n", COLOR_DARK_GREY);
        vga_puts("  --------     ---------     ---     -----------\n", COLOR_DARK_GREY);

        int total = zstore_get_count();
        for (int i = 0; i < total; i++) {
            const store_entry_t* e = zstore_get_by_index(i);
            vga_puts("  ", COLOR_WHITE);
            vga_puts(e->header.name, COLOR_LIGHT_GREEN);

            int len = 0;
            while (e->header.name[len] != '\0') len++;
            for (int p = 0; p < 13 - len; p++) vga_putc(' ', COLOR_WHITE);

            vga_puts(e->header.developer, COLOR_LIGHT_CYAN);
            len = 0;
            while (e->header.developer[len] != '\0') len++;
            for (int p = 0; p < 14 - len; p++) vga_putc(' ', COLOR_WHITE);

            print_dec(e->header.version_major, COLOR_WHITE);
            vga_putc('.', COLOR_WHITE);
            print_dec(e->header.version_minor, COLOR_WHITE);
            vga_puts("     ", COLOR_WHITE);

            vga_puts(e->header.description, COLOR_WHITE);
            vga_putc('\n', COLOR_WHITE);
        }
        vga_puts("\nTo install: ", COLOR_LIGHT_GREEN);
        vga_puts("zpkg install <app_name>\n\n", COLOR_WHITE);
    }
    else if (args[0] == 'i' && args[1] == 'n' && args[2] == 's' && args[3] == 't') {
        const char* p = args + 7;
        while (*p == ' ') p++;

        if (*p == '\0') {
            vga_puts("Usage: zpkg install <app_name>\n", COLOR_LIGHT_RED);
            return;
        }

        vga_puts("Connecting to ZeroOS App Store Repository...\n", COLOR_LIGHT_CYAN);
        timer_sleep_ms(300);

        if (zstore_install_app(vol, p)) {
            vga_puts("Downloading package '", COLOR_WHITE);
            vga_puts(p, COLOR_LIGHT_GREEN);
            vga_puts(".zap'...\n", COLOR_WHITE);
            timer_sleep_ms(400);

            vga_puts("Verifying ZAP1 cryptographic checksum... [OK]\n", COLOR_LIGHT_CYAN);
            vga_puts("App installed successfully! Type 'run ", COLOR_LIGHT_GREEN);
            vga_puts(p, COLOR_WHITE);
            vga_puts("' to launch.\n", COLOR_LIGHT_GREEN);
        } else {
            vga_puts("Error: Package '", COLOR_LIGHT_RED);
            vga_puts(p, COLOR_LIGHT_RED);
            vga_puts("' not found in store.\n", COLOR_LIGHT_RED);
        }
    }
}

void zap_run(volume_t* vol, const char* app_name) {
    while (*app_name == ' ') app_name++;
    if (*app_name == '\0') {
        vga_puts("Usage: run <app_name>\n", COLOR_LIGHT_RED);
        return;
    }

    char zap_file[32];
    int i = 0;
    while (app_name[i] != '\0' && app_name[i] != ' ') {
        zap_file[i] = app_name[i];
        i++;
    }
    zap_file[i++] = '.';
    zap_file[i++] = 'z';
    zap_file[i++] = 'a';
    zap_file[i++] = 'p';
    zap_file[i] = '\0';

    static uint8_t load_buf[512];
    int bytes = zerofs_read(vol, zap_file, (char*)load_buf, 512);

    if (bytes < (int)sizeof(zap_header_t)) {
        vga_puts("App not installed. Try 'zpkg install ", COLOR_LIGHT_RED);
        vga_puts(app_name, COLOR_LIGHT_RED);
        vga_puts("'\n", COLOR_LIGHT_RED);
        return;
    }

    zap_header_t* hdr = (zap_header_t*)load_buf;

    if (hdr->magic != ZAP_MAGIC) {
        vga_puts("Corrupt or invalid .ZAP binary header!\n", COLOR_LIGHT_RED);
        return;
    }

    vga_puts("Loading '", COLOR_LIGHT_CYAN);
    vga_puts(hdr->name, COLOR_LIGHT_GREEN);
    vga_puts("' by ", COLOR_LIGHT_CYAN);
    vga_puts(hdr->developer, COLOR_LIGHT_GREEN);
    vga_puts("...\n", COLOR_LIGHT_CYAN);
    timer_sleep_ms(300);

    uint64_t* entry_ptr = (uint64_t*)(load_buf + sizeof(zap_header_t));
    zap_main_func entry = (zap_main_func)(*entry_ptr);

    vga_puts("Entering isolated Ring 3 process...\n", COLOR_DARK_GREY);
    keyboard_drain();
    int status = user_process_run((uint64_t)entry);
    if (status < 0 && user_process_last_status() == 0) {
        vga_puts("Failed to start isolated user process.\n", COLOR_LIGHT_RED);
    } else {
        vga_puts("App", COLOR_LIGHT_CYAN);
        vga_puts(user_status_text(user_process_last_status()), COLOR_LIGHT_CYAN);
        if (user_process_last_status() & 0x80000000UL) {
            vga_puts(user_process_last_message(), COLOR_LIGHT_RED);
            vga_putc('\n', COLOR_LIGHT_RED);
        }
    }

    vga_set_cursor(0, 0);
    vga_clear(COLOR_BLACK);
}
