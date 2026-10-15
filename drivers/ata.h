#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

int ata_identify(uint8_t bus, uint8_t drive);
int ata_read_sectors(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t count, uint8_t* buffer);
int ata_write_sectors(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer);

#endif
