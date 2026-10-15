#include "ata.h"

static const uint16_t ATA_BASE_PORTS[2] = { 0x1F0, 0x170 };

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static void ata_wait_bsy(uint16_t base) {
    while (inb(base + 7) & 0x80);
}

static void ata_wait_drq(uint16_t base) {
    while (!(inb(base + 7) & 0x08));
}

int ata_identify(uint8_t bus, uint8_t drive) {
    uint16_t base = ATA_BASE_PORTS[bus];

    outb(base + 6, drive ? 0xB0 : 0xA0);
    outb(base + 2, 0);
    outb(base + 3, 0);
    outb(base + 4, 0);
    outb(base + 5, 0);
    outb(base + 7, 0xEC);

    uint8_t status = inb(base + 7);
    if (status == 0) return 0;

    ata_wait_bsy(base);

    if (inb(base + 4) != 0 || inb(base + 5) != 0) return 0;

    while (1) {
        status = inb(base + 7);
        if (status & 0x01) return 0;
        if (status & 0x08) break;
    }

    for (int i = 0; i < 256; i++) {
        inw(base);
    }
    return 1;
}

int ata_read_sectors(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t count, uint8_t* buffer) {
    uint16_t base = ATA_BASE_PORTS[bus];

    ata_wait_bsy(base);

    outb(base + 6, (drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    outb(base + 1, 0x00);
    outb(base + 2, count);
    outb(base + 3, (uint8_t)(lba & 0xFF));
    outb(base + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(base + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(base + 7, 0x20);

    uint16_t* target = (uint16_t*)buffer;

    for (int s = 0; s < count; s++) {
        ata_wait_bsy(base);
        ata_wait_drq(base);

        for (int i = 0; i < 256; i++) {
            *target++ = inw(base);
        }
    }
    return 1;
}

int ata_write_sectors(uint8_t bus, uint8_t drive, uint32_t lba, uint8_t count, const uint8_t* buffer) {
    uint16_t base = ATA_BASE_PORTS[bus];

    ata_wait_bsy(base);

    outb(base + 6, (drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F));
    outb(base + 1, 0x00);
    outb(base + 2, count);
    outb(base + 3, (uint8_t)(lba & 0xFF));
    outb(base + 4, (uint8_t)((lba >> 8) & 0xFF));
    outb(base + 5, (uint8_t)((lba >> 16) & 0xFF));
    outb(base + 7, 0x30);

    const uint16_t* src = (const uint16_t*)buffer;

    for (int s = 0; s < count; s++) {
        ata_wait_bsy(base);
        ata_wait_drq(base);

        for (int i = 0; i < 256; i++) {
            outw(base, *src++);
        }
    }

    outb(base + 7, 0xE7);
    ata_wait_bsy(base);
    return 1;
}
