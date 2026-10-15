#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PCI class codes */
#define PCI_CLASS_NETWORK   0x02
#define PCI_CLASS_STORAGE   0x01
#define PCI_CLASS_BRIDGE    0x06
#define PCI_CLASS_DISPLAY   0x03
#define PCI_CLASS_MULTIMEDIA 0x04

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;
    uint32_t bar[6];
    uint8_t  irq;
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    int      enabled;
} pci_device_t;

#define PCI_MAX_DEVICES 32

void pci_init(void);
int  pci_scan(pci_device_t* out, int max_devices);
int  pci_device_count(void);
pci_device_t* pci_find_class(uint8_t class_code, uint8_t subclass);
pci_device_t* pci_find_device(uint16_t vendor, uint16_t device);
void pci_enable_bus_master(pci_device_t* dev);
void pci_dump(void);
const char* pci_class_name(uint8_t class_code);

#endif
