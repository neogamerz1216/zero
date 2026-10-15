#ifndef ZAP_H
#define ZAP_H

#include <stdint.h>

#define ZAP_MAGIC 0x5A415031

typedef struct {
    uint32_t magic;
    char name[20];
    char developer[20];
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t entry_offset;
    uint32_t payload_size;
    char description[64];
} __attribute__((packed)) zap_header_t;

typedef struct {
    void (*print)(const char* str, uint8_t color);
    void (*putc)(char c, uint8_t color);
    void (*clear)(uint8_t bg);
    char (*getchar)(void);
    void (*sleep)(uint32_t ms);
    void (*beep)(uint32_t freq, uint32_t ms);
    void (*set_cursor)(int x, int y);
    void* reserved;
} zap_api_t;

typedef int (*zap_main_func)(const zap_api_t* api);

#endif
