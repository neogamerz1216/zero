#include "zap.h"
#include "../drivers/vga.h"

int app_matrix_main(const zap_api_t* api) {
    api->clear(COLOR_BLACK);
    api->print("=== Matrix Digital Rain | Running Live ===\n", COLOR_LIGHT_GREEN);

    uint8_t columns[80];
    for (int i = 0; i < 80; i++) columns[i] = (i * 7) % 25;

    for (int frame = 0; frame < 120; frame++) {
        for (int x = 0; x < 80; x += 2) {
            int y = columns[x];
            api->set_cursor(x, y);
            char glyph = 'A' + ((frame + x) % 26);
            api->putc(glyph, COLOR_WHITE);

            if (y > 0) {
                api->set_cursor(x, y - 1);
                api->putc('0' + ((frame * 3 + x) % 10), COLOR_LIGHT_GREEN);
            }

            columns[x] = (columns[x] + 1) % 24;
        }
        api->sleep(40);
    }

    api->clear(COLOR_BLACK);
    return 0;
}

int app_tune_main(const zap_api_t* api) {
    api->clear(COLOR_BLACK);
    api->print("\n=== ZeroTone 8-Bit Chiptune Player ===\n", COLOR_LIGHT_CYAN);
    api->print("Playing Retro Synth Melody...\n\n", COLOR_WHITE);

    uint32_t notes[] = { 261, 293, 329, 349, 392, 440, 493, 523 };
    for (int i = 0; i < 8; i++) {
        api->print(">> Note: ", COLOR_LIGHT_GREEN);
        api->putc('1' + i, COLOR_WHITE);
        api->print(" Played\n", COLOR_LIGHT_GREEN);
        api->beep(notes[i], 120);
        api->sleep(40);
    }

    api->print("\nPlayback Complete. Returning to Shell.\n", COLOR_LIGHT_CYAN);
    api->sleep(600);
    return 0;
}

int app_sysmon_main(const zap_api_t* api) {
    api->clear(COLOR_BLACK);
    api->print("====================================================\n", COLOR_LIGHT_CYAN);
    api->print("          ZeroOS Live System Resource Monitor       \n", COLOR_WHITE);
    api->print("====================================================\n", COLOR_LIGHT_CYAN);
    api->print("  Status      : Online (Supervised)\n", COLOR_LIGHT_GREEN);
    api->print("  Architecture: AMD64 / x86_64\n", COLOR_WHITE);
    api->print("  RAM Cache   : Drive Aa: Active (1MB Fast Memory)\n", COLOR_WHITE);
    api->print("  Storage     : Drive A: Formatted with ZeroFS\n\n", COLOR_WHITE);
    api->print("Press any key to exit monitor...\n", COLOR_DARK_GREY);
    api->getchar();
    return 0;
}
