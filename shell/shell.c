#include "shell.h"
#include "zedit.h"
#include "zpkg.h"
#include "../apps/zstore.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../drivers/speaker.h"
#include "../drivers/serial.h"
#include "../drivers/volume.h"
#include "../drivers/ata.h"
#include "../fs/zerofs.h"
#include <stdint.h>

static uint8_t prompt_color = COLOR_LIGHT_CYAN;
static uint8_t text_color   = COLOR_WHITE;
static volume_t* current_drive = 0;

static int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

static void print_dec(int64_t val, uint8_t color) {
    char buf[32]; int i = 0;
    if (val == 0) { vga_putc('0', color); return; }
    if (val < 0) { vga_putc('-', color); val = -val; }
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putc(buf[j], color);
}

static void print_2digit(uint8_t val, uint8_t color) {
    vga_putc('0' + (val / 10), color);
    vga_putc('0' + (val % 10), color);
}

static void print_hex(uint64_t val, uint8_t color) {
    const char hex[] = "0123456789ABCDEF";
    vga_puts("0x", color);
    for (int i = 60; i >= 0; i -= 4) {
        vga_putc(hex[(val >> i) & 0xF], color);
    }
}

static void reboot_system(void) {
    vga_puts("\nRebooting ZeroOS hardware...\n", COLOR_LIGHT_RED);
    timer_sleep_ms(500);

    uint8_t good = 0x02;
    while (good & 0x02) {
        __asm__ volatile ("inb %1, %0" : "=a"(good) : "Nd"(0x64));
    }
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));

    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void show_banner(void) {
    vga_puts("  ______                   ____   _____ \n", COLOR_LIGHT_CYAN);
    vga_puts(" |___  /                  / __ \\ / ____|\n", COLOR_LIGHT_CYAN);
    vga_puts("    / /  ___ _ __ ___    | |  | | (___  \n", COLOR_LIGHT_CYAN);
    vga_puts("   / /  / _ \\ '__/ _ \\   | |  | |\\___ \\ \n", COLOR_CYAN);
    vga_puts("  / /__|  __/ | | (_) |  | |__| |____) |\n", COLOR_CYAN);
    vga_puts(" /_____|\\___|_|  \\___/    \\____/|_____/ \n\n", COLOR_CYAN);
    vga_puts(" >> Enhanced By ZeroOS <<\n", COLOR_LIGHT_GREEN);
    vga_puts(" Modern Process-Driven Architecture | ZeroFS & App Store Online\n\n", COLOR_DARK_GREY);
}

static void cmd_sysinfo(void) {
    vga_puts("\n[ ZeroOS System Information ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  Build Version : Enhanced By ZeroOS (v1.0 Release)\n", COLOR_LIGHT_GREEN);
    vga_puts("  Architecture  : x86_64 Long Mode (Ring 0 Supervised)\n", text_color);
    vga_puts("  Drivers Loaded: VGA, PIT(100Hz), PS/2, 8259 PIC, RTC, Speaker, COM1, ATA/IDE\n", text_color);
    vga_puts("  Storage Model : Physical Volumes (A:-D:) & RAM Drives (Aa:, Ba:)\n", text_color);
    vga_puts("  Filesystem    : ZeroFS Inode File Allocation Engine\n", text_color);
    vga_puts("  Diagnostics   : Serial COM1 Output Active at 38400 Baud\n\n", text_color);
}

static void cmd_memmap(void) {
    uint32_t count = *(uint32_t*)0x7000;
    vga_puts("\n[ E820 BIOS Memory Layout Map ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  Total Detected Map Entries: ", text_color);
    print_dec(count, COLOR_LIGHT_GREEN);
    vga_puts("\n\n  ENTRY  BASE ADDRESS         LENGTH               TYPE\n", COLOR_DARK_GREY);

    struct e820_entry {
        uint64_t base;
        uint64_t length;
        uint32_t type;
        uint32_t acpi;
    } __attribute__((packed))* map = (struct e820_entry*)0x5000;

    for (uint32_t i = 0; i < count && i < 8; i++) {
        vga_puts("  [", COLOR_DARK_GREY);
        print_dec(i, text_color);
        vga_puts("]   ", COLOR_DARK_GREY);
        print_hex(map[i].base, text_color);
        vga_puts("   ", text_color);
        print_hex(map[i].length, text_color);
        vga_puts("   ", text_color);

        if (map[i].type == 1)      vga_puts("USABLE RAM\n", COLOR_LIGHT_GREEN);
        else if (map[i].type == 2) vga_puts("RESERVED\n", COLOR_LIGHT_RED);
        else if (map[i].type == 3) vga_puts("ACPI RECLAIM\n", COLOR_LIGHT_CYAN);
        else                       vga_puts("HARDWARE\n", COLOR_DARK_GREY);
    }
    vga_puts("\n", text_color);
}

static void cmd_tasks(void) {
    vga_puts("\n[ ZeroOS Active Processes ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  PID   TASK NAME        STATE       RING\n", COLOR_DARK_GREY);
    vga_puts("  ---   ---------        -----       ----\n", COLOR_DARK_GREY);
    vga_puts("   0    kernel_idle      READY       Ring 0 (Supervisor)\n", COLOR_LIGHT_GREEN);
    vga_puts("   1    shell_task       RUNNING     Ring 0 (Supervisor)\n\n", COLOR_LIGHT_GREEN);
}

static void cmd_drives(void) {
    vga_puts("\n[ ZeroOS Mounted Storage Volumes ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  DRIVE   TYPE              STATUS        DETAILS\n", COLOR_DARK_GREY);
    vga_puts("  -----   ----              ------        -------\n", COLOR_DARK_GREY);

    const char* drive_names[] = { "A:", "B:", "C:", "D:", "Aa:", "Ba:" };
    for (int i = 0; i < 6; i++) {
        volume_t* v = volume_get(drive_names[i]);
        if (!v) continue;

        vga_puts("   ", text_color);
        vga_puts(v->name, COLOR_LIGHT_GREEN);
        if (strlen(v->name) == 2) vga_putc(' ', text_color);
        vga_puts("    ", text_color);

        if (v->type == VOL_TYPE_DISK) {
            vga_puts("Physical IDE/ATA  ", text_color);
            if (v->is_online) {
                vga_puts("ONLINE        ", COLOR_LIGHT_GREEN);
                vga_puts("Primary Boot Disk (32MB)\n", COLOR_WHITE);
            } else {
                vga_puts("OFFLINE       ", COLOR_DARK_GREY);
                vga_puts("No device detected\n", COLOR_DARK_GREY);
            }
        } else {
            vga_puts("Memory RAM Disk   ", COLOR_LIGHT_CYAN);
            vga_puts("ACTIVE        ", COLOR_LIGHT_GREEN);
            if (i == 4) vga_puts("Base: 0x400000 (1MB)\n", COLOR_WHITE);
            else        vga_puts("Base: 0x500000 (2MB)\n", COLOR_WHITE);
        }
    }
    vga_puts("\n", text_color);
}

static void cmd_theme(const char* arg) {
    if (strcmp(arg, "green") == 0 || strcmp(arg, "matrix") == 0) {
        prompt_color = COLOR_LIGHT_GREEN;
        text_color   = COLOR_LIGHT_GREEN;
        vga_puts("Theme set to Matrix Green.\n", COLOR_LIGHT_GREEN);
    } else if (strcmp(arg, "amber") == 0) {
        prompt_color = COLOR_LIGHT_BROWN;
        text_color   = COLOR_LIGHT_BROWN;
        vga_puts("Theme set to Amber Glow.\n", COLOR_LIGHT_BROWN);
    } else {
        prompt_color = COLOR_LIGHT_CYAN;
        text_color   = COLOR_WHITE;
        vga_puts("Theme set to Cyber Cyan.\n", COLOR_LIGHT_CYAN);
    }
}

static void cmd_calc(const char* args) {
    while (*args == ' ') args++;
    if (*args == '\0') {
        vga_puts("Usage: calc <number> <+|-|*|/> <number>\n", COLOR_LIGHT_RED);
        return;
    }

    int64_t n1 = 0;
    while (*args >= '0' && *args <= '9') {
        n1 = (n1 * 10) + (*args - '0');
        args++;
    }

    while (*args == ' ') args++;
    char op = *args;
    if (op == '\0') {
        vga_puts("Missing operator. Usage: calc <num> <+|-|*|/> <num>\n", COLOR_LIGHT_RED);
        return;
    }
    args++;

    while (*args == ' ') args++;
    int64_t n2 = 0;
    while (*args >= '0' && *args <= '9') {
        n2 = (n2 * 10) + (*args - '0');
        args++;
    }

    int64_t res = 0;
    if (op == '+') res = n1 + n2;
    else if (op == '-') res = n1 - n2;
    else if (op == '*') res = n1 * n2;
    else if (op == '/') {
        if (n2 == 0) {
            vga_puts("Error: Division by zero\n", COLOR_LIGHT_RED);
            return;
        }
        res = n1 / n2;
    } else {
        vga_puts("Unsupported operator. Use +, -, *, or /\n", COLOR_LIGHT_RED);
        return;
    }

    vga_puts("= ", COLOR_LIGHT_CYAN);
    print_dec(res, COLOR_LIGHT_GREEN);
    vga_putc('\n', text_color);
}

static void cmd_help(void) {
    vga_puts("\n[ ZeroOS Master Command Directory ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  Filesystem Directives:\n", COLOR_LIGHT_GREEN);
    vga_puts("    ls / dir          - List files stored on active drive\n", text_color);
    vga_puts("    format            - Format active drive with fresh ZeroFS\n", text_color);
    vga_puts("    touch <filename>  - Create a new empty file\n", text_color);
    vga_puts("    write <file> <txt>- Write inline sentence into file\n", text_color);
    vga_puts("    cat <filename>    - Read and display text content of file\n", text_color);
    vga_puts("    rm <filename>     - Delete file from filesystem\n", text_color);
    vga_puts("    zedit <filename>  - Full-screen text editor (ESC to save)\n", text_color);
    vga_puts("  App Store & Package Manager:\n", COLOR_LIGHT_GREEN);
    vga_puts("    zpkg / store      - Browse apps in Central App Store\n", text_color);
    vga_puts("    zpkg install <app>- Install an application to disk\n", text_color);
    vga_puts("    run <app_name>    - Launch an installed ZeroOS application\n", text_color);
    vga_puts("  System Directives:\n", COLOR_LIGHT_GREEN);
    vga_puts("    drives            - Display disks (A-D) & RAM (Aa, Ba)\n", text_color);
    vga_puts("    drive <name>      - Switch active drive (e.g. drive A: or drive Aa:)\n", text_color);
    vga_puts("    time              - Query CMOS hardware clock\n", text_color);
    vga_puts("    beep              - Play PC speaker tone\n", text_color);
    vga_puts("    sysinfo           - Display kernel & hardware specs\n", text_color);
    vga_puts("    memmap            - Display E820 physical RAM sectors\n", text_color);
    vga_puts("    tasks             - List active Process Control Blocks\n", text_color);
    vga_puts("    calc <n1> <op> <n2>- Integer calculator\n", text_color);
    vga_puts("    theme <name>      - Palette: cyan, green, amber\n", text_color);
    vga_puts("    wipe              - Clear screen\n", text_color);
    vga_puts("    reboot            - Cold hardware restart\n\n", text_color);
}

void shell_main(void) {
    vga_clear(COLOR_BLACK);
    vga_enable_cursor(14, 15);
    show_banner();

    current_drive = volume_get("A:");
    if (!current_drive || !current_drive->is_online) {
        current_drive = volume_get("Aa:");
    }
    zerofs_init(current_drive);
    zstore_init();

    vga_puts("Active Drive: ", COLOR_LIGHT_CYAN);
    vga_puts(current_drive->name, COLOR_LIGHT_GREEN);
    vga_puts(" | Type 'help' for directory.\n\n", COLOR_DARK_GREY);

    char line[128];
    int idx = 0;

    while (1) {
        vga_puts(current_drive->name, COLOR_LIGHT_GREEN);
        vga_puts("ZeroOS", prompt_color);
        vga_puts("> ", COLOR_WHITE);
        idx = 0;

        while (1) {
            char c = keyboard_getchar();

            if (c == '\n') {
                line[idx] = '\0';
                vga_putc('\n', text_color);
                break;
            } else if (c == '\b') {
                if (idx > 0) {
                    idx--;
                    vga_putc('\b', text_color);
                }
            } else if (idx < 127) {
                line[idx++] = c;
                vga_putc(c, text_color);
            }
        }

        int i = 0;
        while (line[i] == ' ') i++;
        char* cmd = &line[i];
        char* arg = "";

        while (line[i] != '\0') {
            if (line[i] == ' ') {
                line[i] = '\0';
                arg = &line[i + 1];
                while (*arg == ' ') arg++;
                break;
            }
            i++;
        }

        if (strlen(cmd) == 0) continue;

        if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
            zerofs_list(current_drive);
        } else if (strcmp(cmd, "format") == 0) {
            if (zerofs_format(current_drive, "ZeroDrive")) {
                vga_puts("Drive formatted with ZeroFS successfully!\n", COLOR_LIGHT_GREEN);
            } else {
                vga_puts("Formatting failed.\n", COLOR_LIGHT_RED);
            }
        } else if (strcmp(cmd, "touch") == 0) {
            if (strlen(arg) == 0) vga_puts("Usage: touch <filename>\n", COLOR_LIGHT_RED);
            else if (zerofs_create(current_drive, arg)) vga_puts("File created.\n", COLOR_LIGHT_GREEN);
            else vga_puts("Failed to create file.\n", COLOR_LIGHT_RED);
        } else if (strcmp(cmd, "write") == 0) {
            char* fname = arg;
            while (*arg != ' ' && *arg != '\0') arg++;
            if (*arg == ' ') {
                *arg = '\0';
                char* text = arg + 1;
                while (*text == ' ') text++;
                if (zerofs_write(current_drive, fname, text, strlen(text))) {
                    vga_puts("Data written to file.\n", COLOR_LIGHT_GREEN);
                } else {
                    vga_puts("Write error.\n", COLOR_LIGHT_RED);
                }
            } else {
                vga_puts("Usage: write <file> <text>\n", COLOR_LIGHT_RED);
            }
        } else if (strcmp(cmd, "cat") == 0) {
            static char read_buf[2048];
            int read_len = zerofs_read(current_drive, arg, read_buf, 2048);
            if (read_len >= 0) {
                vga_puts(read_buf, COLOR_WHITE);
                vga_putc('\n', COLOR_WHITE);
            } else {
                vga_puts("File not found.\n", COLOR_LIGHT_RED);
            }
        } else if (strcmp(cmd, "rm") == 0) {
            if (zerofs_delete(current_drive, arg)) vga_puts("File deleted.\n", COLOR_LIGHT_GREEN);
            else vga_puts("File not found.\n", COLOR_LIGHT_RED);
        } else if (strcmp(cmd, "zedit") == 0) {
            if (strlen(arg) == 0) vga_puts("Usage: zedit <filename>\n", COLOR_LIGHT_RED);
            else zedit_open(current_drive, arg);
        } else if (strcmp(cmd, "zpkg") == 0 || strcmp(cmd, "store") == 0) {
            zpkg_cli(current_drive, arg);
        } else if (strcmp(cmd, "run") == 0) {
            zap_run(current_drive, arg);
        } else if (strcmp(cmd, "drive") == 0) {
            volume_t* v = volume_get(arg);
            if (v && v->is_online) {
                current_drive = v;
                zerofs_init(current_drive);
                vga_puts("Switched drive to ", COLOR_LIGHT_GREEN);
                vga_puts(current_drive->name, COLOR_LIGHT_GREEN);
                vga_putc('\n', COLOR_WHITE);
            } else {
                vga_puts("Drive not found or offline.\n", COLOR_LIGHT_RED);
            }
        } else if (strcmp(cmd, "drives") == 0) {
            cmd_drives();
        } else if (strcmp(cmd, "sysinfo") == 0) {
            cmd_sysinfo();
        } else if (strcmp(cmd, "memmap") == 0) {
            cmd_memmap();
        } else if (strcmp(cmd, "tasks") == 0) {
            cmd_tasks();
        } else if (strcmp(cmd, "theme") == 0) {
            cmd_theme(arg);
        } else if (strcmp(cmd, "calc") == 0) {
            cmd_calc(arg);
        } else if (strcmp(cmd, "time") == 0) {
            rtc_time_t t = rtc_get_time();
            vga_puts("Current Hardware RTC Time: ", COLOR_LIGHT_CYAN);
            print_2digit(t.hour, text_color);
            vga_putc(':', text_color);
            print_2digit(t.minute, text_color);
            vga_putc(':', text_color);
            print_2digit(t.second, text_color);
            vga_puts("  Date: ", COLOR_LIGHT_CYAN);
            print_2digit(t.day, text_color);
            vga_putc('/', text_color);
            print_2digit(t.month, text_color);
            vga_putc('/', text_color);
            print_dec(t.year, text_color);
            vga_putc('\n', text_color);
        } else if (strcmp(cmd, "beep") == 0) {
            speaker_play_startup_jingle();
        } else if (strcmp(cmd, "wipe") == 0 || strcmp(cmd, "clear") == 0) {
            vga_clear(COLOR_BLACK);
        } else if (strcmp(cmd, "reboot") == 0) {
            reboot_system();
        } else {
            vga_puts("Unknown directive: '", COLOR_LIGHT_RED);
            vga_puts(cmd, COLOR_LIGHT_RED);
            vga_puts("'. Enter 'help' for directory.\n", COLOR_LIGHT_RED);
        }
    }
}
