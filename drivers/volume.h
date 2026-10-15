#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>

typedef enum {
    VOL_TYPE_DISK,
    VOL_TYPE_RAM
} vol_type_t;

typedef struct {
    char name[4];
    vol_type_t type;
    int is_online;
    uint8_t ata_bus;
    uint8_t ata_drive;
    uint64_t ram_base;
    uint64_t size_bytes;
} volume_t;

void volume_init(void);
volume_t* volume_get(const char* name);
int volume_read(volume_t* vol, uint32_t sector, uint8_t* buffer);
int volume_write(volume_t* vol, uint32_t sector, const uint8_t* buffer);

#endif
