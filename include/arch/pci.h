#ifndef ARCH_PCI_H
#define ARCH_PCI_H

#include <stdint.h>

/*
 * PCI configuration space access, "mechanism #1": write a bus/device/
 * function/offset address to port 0xCF8, then read the data from port 0xCFC.
 * Every PCI device exposes 256 bytes of configuration (IDs, class, BARs)
 * that the OS reads to find out what is plugged in and where it lives.
 */

#define PCI_MAX_DEVICES 32

struct pci_device {
    uint8_t bus, dev, func;
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if;
    uint8_t header_type;
};

uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/* Enumerate every bus; returns how many devices were stored in `out`. */
int pci_scan(struct pci_device *out, int max);

const char *pci_class_name(uint8_t class_code, uint8_t subclass);
const char *pci_vendor_name(uint16_t vendor);

#endif
