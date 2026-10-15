#include "drivman.h"
#include "../drivers/vga.h"

static drv_entry_t drv_table[DRIVMAN_MAX_DRIVERS];
static int drv_count = 0;

void drvman_init(void) {
    drv_count = 0;
    for (int i = 0; i < DRIVMAN_MAX_DRIVERS; i++) {
        drv_table[i].name[0] = 0;
        drv_table[i].init = 0;
        drv_table[i].installed = 0;
        drv_table[i].initialized = 0;
    }
}

int drvman_register(const char* name, drv_init_fn init) {
    if (drv_count >= DRIVMAN_MAX_DRIVERS) return -1;
    if (!name || !init) return -1;
    drv_entry_t* d = &drv_table[drv_count];
    int i = 0;
    while (name[i] && i < 31) {
        d->name[i] = name[i];
        i++;
    }
    d->name[i] = 0;
    d->init = init;
    d->installed = 1;
    d->initialized = 0;
    drv_count++;
    return drv_count - 1;
}

int drvman_load(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < drv_count; i++) {
        int match = 1;
        for (int j = 0; name[j]; j++) {
            if (drv_table[i].name[j] != name[j]) { match = 0; break; }
        }
        if (!match || drv_table[i].name[0] == 0) continue;
        if (!drv_table[i].installed) return -2;
        if (drv_table[i].initialized) return 1; /* already */
        if (!drv_table[i].init) return -3;
        int rc = drv_table[i].init();
        drv_table[i].initialized = (rc == 0);
        return rc;
    }
    return -4; /* not found */
}

int drvman_init_all(void) {
    int ok = 0, fail = 0;
    for (int i = 0; i < drv_count; i++) {
        if (!drv_table[i].installed) continue;
        if (drv_table[i].initialized) continue;
        if (!drv_table[i].init) continue;
        int rc = drv_table[i].init();
        drv_table[i].initialized = (rc == 0);
        if (rc == 0) ok++; else fail++;
    }
    return ok;
}

void drvman_list(void) {
    vga_puts("\n[ ZeroOS Driver Registry ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  #  DRIVER NAME              STATE\n", COLOR_DARK_GREY);
    vga_puts("  --  ------------            -----\n", COLOR_DARK_GREY);
    for (int i = 0; i < drv_count; i++) {
        drv_entry_t* d = &drv_table[i];
        vga_puts("  ", COLOR_WHITE);
        char num[4]; num[0] = '0' + i / 100; num[1] = '0' + (i / 10) % 10;
        num[2] = '0' + i % 10; num[3] = 0;
        vga_puts(num, COLOR_LIGHT_GREEN);
        vga_puts("   ", COLOR_WHITE);
        vga_puts(d->name, COLOR_LIGHT_CYAN);
        /* pad */
        int len = 0;
        while (d->name[len]) len++;
        for (int j = len; j < 26; j++) vga_putc(' ', COLOR_WHITE);
        if (d->initialized)
            vga_puts("INITIALIZED\n", COLOR_LIGHT_GREEN);
        else if (d->installed)
            vga_puts("INSTALLED (pending)\n", COLOR_LIGHT_BROWN);
        else
            vga_puts("NOT INSTALLED\n", COLOR_LIGHT_RED);
    }
    vga_puts("\n", COLOR_WHITE);
}

int drvman_count(void) {
    return drv_count;
}
