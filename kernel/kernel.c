#include "../drivers/vga.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../drivers/speaker.h"
#include "../drivers/serial.h"
#include "../drivers/volume.h"
#include "pic.h"
#include "idt.h"
#include "process.h"
#include "mm.h"
#include "syscall.h"
#include "../shell/shell.h"

void kernel_main(void) {
    serial_init();
    serial_puts("\n[ZeroOS Kernel] 64-Bit Long Mode Boot Initiated.\n");
    serial_puts("[ZeroOS Kernel] Watermark Check: Enhanced By ZeroOS Verified.\n");

    vga_init();
    vga_show_splash();

    idt_init();
    pic_remap();
    serial_puts("[ZeroOS Kernel] IDT & PIC Remapped.\n");

    pmm_init();
    if (mm_selftest()) {
        serial_puts("[ZeroOS Kernel] PMM and heap self-test passed.\n");
    } else {
        serial_puts("[ZeroOS Kernel] PMM or heap self-test FAILED.\n");
    }
    syscall_init();
    serial_puts("[ZeroOS Kernel] Ring 3 selectors and syscall entry armed.\n");

    timer_init(100);
    rtc_init();
    __asm__ volatile ("sti");

    vga_update_boot_step("Configuring 8259 PIC & RTC Clock...", 20);
    serial_puts("[ZeroOS Kernel] RTC & PIT Online.\n");
    timer_sleep_ms(600);

    vga_update_boot_step("Activating PS/2 Controller & Buffers...", 40);
    keyboard_init();
    serial_puts("[ZeroOS Kernel] PS/2 Input Initialized.\n");
    timer_sleep_ms(600);

    vga_update_boot_step("Testing PC Speaker Hardware Audio...", 60);
    speaker_play_startup_jingle();
    serial_puts("[ZeroOS Kernel] Audio Jingle Completed.\n");
    timer_sleep_ms(400);

    vga_update_boot_step("Mounting Volumes [A-D, Aa, Ba]...", 80);
    volume_init();
    serial_puts("[ZeroOS Kernel] Volumes Initialized.\n");
    timer_sleep_ms(600);

    vga_update_boot_step("Starting Process Engine & Shell Task...", 100);
    process_init();
    process_create(shell_main);
    serial_puts("[ZeroOS Kernel] Process 1 Spawned. Entering Kernel Idle Loop.\n");
    timer_sleep_ms(800);

    while (1) {
        __asm__ volatile ("sti; hlt");
    }
}
