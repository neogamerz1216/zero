#include "volume.h"
#include "ata.h"

#define MAX_VOLUMES 6
static volume_t volumes[MAX_VOLUMES];

void volume_init(void) {
    volumes[0].name[0] = 'A'; volumes[0].name[1] = ':'; volumes[0].name[2] = '\0';
    volumes[0].type = VOL_TYPE_DISK;
    volumes[0].ata_bus = 0; volumes[0].ata_drive = 0;
    volumes[0].is_online = ata_identify(0, 0);
    volumes[0].size_bytes = 32 * 1024 * 1024;

    volumes[1].name[0] = 'B'; volumes[1].name[1] = ':'; volumes[1].name[2] = '\0';
    volumes[1].type = VOL_TYPE_DISK;
    volumes[1].ata_bus = 0; volumes[1].ata_drive = 1;
    volumes[1].is_online = ata_identify(0, 1);

    volumes[2].name[0] = 'C'; volumes[2].name[1] = ':'; volumes[2].name[2] = '\0';
    volumes[2].type = VOL_TYPE_DISK;
    volumes[2].ata_bus = 1; volumes[2].ata_drive = 0;
    volumes[2].is_online = ata_identify(1, 0);

    volumes[3].name[0] = 'D'; volumes[3].name[1] = ':'; volumes[3].name[2] = '\0';
    volumes[3].type = VOL_TYPE_DISK;
    volumes[3].ata_bus = 1; volumes[3].ata_drive = 1;
    volumes[3].is_online = ata_identify(1, 1);

    volumes[4].name[0] = 'A'; volumes[4].name[1] = 'a'; volumes[4].name[2] = ':'; volumes[4].name[3] = '\0';
    volumes[4].type = VOL_TYPE_RAM;
    volumes[4].is_online = 1;
    volumes[4].ram_base = 0x400000;
    volumes[4].size_bytes = 1024 * 1024;

    volumes[5].name[0] = 'B'; volumes[5].name[1] = 'a'; volumes[5].name[2] = ':'; volumes[5].name[3] = '\0';
    volumes[5].type = VOL_TYPE_RAM;
    volumes[5].is_online = 1;
    volumes[5].ram_base = 0x500000;
    volumes[5].size_bytes = 2 * 1024 * 1024;
}

static int str_eq(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

volume_t* volume_get(const char* name) {
    for (int i = 0; i < MAX_VOLUMES; i++) {
        if (str_eq(volumes[i].name, name) == 0) {
            return &volumes[i];
        }
    }
    return 0;
}

int volume_read(volume_t* vol, uint32_t sector, uint8_t* buffer) {
    if (!vol || !vol->is_online) return 0;

    if (vol->type == VOL_TYPE_DISK) {
        return ata_read_sectors(vol->ata_bus, vol->ata_drive, sector, 1, buffer);
    } else {
        uint8_t* src = (uint8_t*)(vol->ram_base + (sector * ATA_SECTOR_SIZE));
        for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
            buffer[i] = src[i];
        }
        return 1;
    }
}

int volume_write(volume_t* vol, uint32_t sector, const uint8_t* buffer) {
    if (!vol || !vol->is_online) return 0;

    if (vol->type == VOL_TYPE_DISK) {
        return ata_write_sectors(vol->ata_bus, vol->ata_drive, sector, 1, buffer);
    } else {
        uint8_t* dst = (uint8_t*)(vol->ram_base + (sector * ATA_SECTOR_SIZE));
        for (int i = 0; i < ATA_SECTOR_SIZE; i++) {
            dst[i] = buffer[i];
        }
        return 1;
    }
}
