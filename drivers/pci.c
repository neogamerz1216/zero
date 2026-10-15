#include "pci.h"
#include "../drivers/vga.h"

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static pci_device_t pci_devices[PCI_MAX_DEVICES];
static int pci_dev_count = 0;

static uint32_t pci_config_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC));
}

static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint16_t)(pci_read_dword(bus, slot, func, offset) >> ((offset & 2) * 8));
}

static uint8_t pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint8_t)(pci_read_dword(bus, slot, func, offset) >> ((offset & 3) * 8));
}

static void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

static void pci_print_hex8(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    vga_putc(hex[(val >> 4) & 0xF], COLOR_WHITE);
    vga_putc(hex[val & 0xF], COLOR_WHITE);
}

static void pci_print_hex16(uint16_t val) {
    pci_print_hex8((uint8_t)(val >> 8));
    pci_print_hex8((uint8_t)val);
}

void pci_init(void) {
    pci_dev_count = 0;
    for (int i = 0; i < PCI_MAX_DEVICES; i++) {
        pci_devices[i].vendor_id = 0xFFFF;
    }
}

int pci_scan(pci_device_t* out, int max_devices) {
    int found = 0;
    for (int bus = 0; bus < 256 && found < max_devices; bus++) {
        for (int slot = 0; slot < 32 && found < max_devices; slot++) {
            for (int func = 0; func < 8 && found < max_devices; func++) {
                uint16_t vendor = pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0);
                if (vendor == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                pci_device_t* d = &out[found];
                d->bus = bus;
                d->slot = slot;
                d->func = func;
                d->vendor_id = vendor;
                d->device_id = pci_read_word(bus, slot, func, 2);
                d->command = pci_read_word(bus, slot, func, 4);
                d->status = pci_read_word(bus, slot, func, 6);
                d->revision_id = pci_read_byte(bus, slot, func, 8);
                d->prog_if = pci_read_byte(bus, slot, func, 9);
                d->subclass = pci_read_byte(bus, slot, func, 10);
                d->class_code = pci_read_byte(bus, slot, func, 11);
                d->cache_line_size = pci_read_byte(bus, slot, func, 12);
                d->latency_timer = pci_read_byte(bus, slot, func, 13);
                d->header_type = pci_read_byte(bus, slot, func, 14);
                d->bist = pci_read_byte(bus, slot, func, 15);
                for (int b = 0; b < 6; b++) {
                    d->bar[b] = pci_read_dword(bus, slot, func, 0x10 + b * 4);
                }
                d->irq = pci_read_byte(bus, slot, func, 0x3C);
                d->enabled = 0;
                found++;
                if (func == 0 && !(d->header_type & 0x80)) break;
            }
        }
    }
    return found;
}

int pci_device_count(void) {
    return pci_dev_count;
}

pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass) {
    static pci_device_t scanbuf[PCI_MAX_DEVICES];
    int n = pci_scan(scanbuf, PCI_MAX_DEVICES);
    pci_dev_count = n;
    for (int i = 0; i < n; i++) {
        if (scanbuf[i].class_code == class_code &&
            (subclass == 0xFF || scanbuf[i].subclass == subclass)) {
            pci_devices[i] = scanbuf[i];
            return &pci_devices[i];
        }
    }
    return 0;
}

pci_device_t* pci_find_device(uint16_t vendor, uint16_t device) {
    static pci_device_t scanbuf[PCI_MAX_DEVICES];
    int n = pci_scan(scanbuf, PCI_MAX_DEVICES);
    pci_dev_count = n;
    for (int i = 0; i < n; i++) {
        if (scanbuf[i].vendor_id == vendor && scanbuf[i].device_id == device) {
            pci_devices[i] = scanbuf[i];
            return &pci_devices[i];
        }
    }
    return 0;
}

void pci_enable_bus_master(pci_device_t* dev) {
    dev->command |= (1 << 2);
    pci_write_dword(dev->bus, dev->slot, dev->func, 4, dev->command);
    dev->enabled = 1;
}

const char* pci_class_name(uint8_t class_code) {
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage";
        case 0x02: return "Network";
        case 0x03: return "Display";
        case 0x04: return "Multimedia";
        case 0x05: return "Memory";
        case 0x06: return "Bridge";
        case 0x07: return "Simple Comm";
        case 0x08: return "Base System";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus";
        case 0x0D: return "Wireless";
        case 0x0E: return "Intelligent I/O";
        case 0x0F: return "Satellite Comm";
        case 0x10: return "Encryption";
        case 0x11: return "Signal Processing";
        default:   return "Unknown";
    }
}

void pci_dump(void) {
    static pci_device_t scanbuf[PCI_MAX_DEVICES];
    int n = pci_scan(scanbuf, PCI_MAX_DEVICES);
    pci_dev_count = n;
    vga_puts("\n[ PCI Device Enumeration ]\n", COLOR_LIGHT_CYAN);
    vga_puts("  BUS SLOT FUNC VENDOR DEVICE CLASS           IRQ\n", COLOR_DARK_GREY);
    for (int i = 0; i < n; i++) {
        pci_device_t* d = &scanbuf[i];
        vga_puts("  ", COLOR_WHITE);
        pci_print_hex8(d->bus);
        vga_putc(' ', COLOR_WHITE);
        pci_print_hex8(d->slot);
        vga_putc(' ', COLOR_WHITE);
        pci_print_hex8(d->func);
        vga_puts("    ", COLOR_WHITE);
        pci_print_hex16(d->vendor_id);
        vga_puts("   ", COLOR_WHITE);
        pci_print_hex16(d->device_id);
        vga_puts("    ", COLOR_WHITE);
        vga_puts(pci_class_name(d->class_code), COLOR_LIGHT_GREEN);
        vga_puts("   ", COLOR_WHITE);
        pci_print_hex8(d->irq);
        vga_putc('\n', COLOR_WHITE);
    }
    vga_puts("\n", COLOR_WHITE);
}
