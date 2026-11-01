#include "arch/pci.h"
#include "arch/io.h"

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_DATA_PORT    0xCFC

uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)(dev & 0x1F) << 11) |
                       ((uint32_t)(func & 0x07) << 8) | (offset & 0xFC);

    outl(PCI_ADDRESS_PORT, address);
    return inl(PCI_DATA_PORT);
}

static void record(struct pci_device *out, int *count, int max, uint8_t bus,
                   uint8_t dev, uint8_t func) {
    uint32_t id = pci_config_read32(bus, dev, func, 0x00);
    uint32_t cls = pci_config_read32(bus, dev, func, 0x08);
    uint32_t hdr = pci_config_read32(bus, dev, func, 0x0C);

    if (*count >= max) {
        return;
    }

    struct pci_device *d = &out[(*count)++];

    d->bus = bus;
    d->dev = dev;
    d->func = func;
    d->vendor = (uint16_t)(id & 0xFFFF);
    d->device = (uint16_t)(id >> 16);
    d->class_code = (uint8_t)(cls >> 24);
    d->subclass = (uint8_t)(cls >> 16);
    d->prog_if = (uint8_t)(cls >> 8);
    d->header_type = (uint8_t)(hdr >> 16);
}

int pci_scan(struct pci_device *out, int max) {
    int count = 0;

    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            /* A missing device reads back all ones. */
            if ((pci_config_read32((uint8_t)bus, (uint8_t)dev, 0, 0) & 0xFFFF) == 0xFFFF) {
                continue;
            }

            record(out, &count, max, (uint8_t)bus, (uint8_t)dev, 0);

            /* Bit 7 of the header type: this device has functions 1-7 too. */
            if ((pci_config_read32((uint8_t)bus, (uint8_t)dev, 0, 0x0C) >> 16) & 0x80) {
                for (int func = 1; func < 8; func++) {
                    if ((pci_config_read32((uint8_t)bus, (uint8_t)dev, (uint8_t)func, 0) &
                         0xFFFF) != 0xFFFF) {
                        record(out, &count, max, (uint8_t)bus, (uint8_t)dev, (uint8_t)func);
                    }
                }
            }
        }
    }

    return count;
}

const char *pci_class_name(uint8_t c, uint8_t s) {
    switch (c) {
        case 0x00: return s == 0x01 ? "VGA-compatible (legacy)" : "unclassified";
        case 0x01:
            switch (s) {
                case 0x00: return "SCSI controller";
                case 0x01: return "IDE controller";
                case 0x06: return "SATA controller";
                case 0x08: return "NVMe controller";
                default:   return "mass storage";
            }
        case 0x02: return s == 0x00 ? "Ethernet controller" : "network controller";
        case 0x03: return s == 0x00 ? "VGA display controller" : "display controller";
        case 0x04: return "multimedia";
        case 0x05: return "memory controller";
        case 0x06:
            switch (s) {
                case 0x00: return "host bridge";
                case 0x01: return "ISA bridge";
                case 0x04: return "PCI-to-PCI bridge";
                default:   return "bridge";
            }
        case 0x07: return "communication controller";
        case 0x08: return "system peripheral";
        case 0x0C: return s == 0x03 ? "USB controller" : "serial bus controller";
        default:   return "other";
    }
}

const char *pci_vendor_name(uint16_t v) {
    switch (v) {
        case 0x8086: return "Intel";
        case 0x1022: return "AMD";
        case 0x1002: return "AMD/ATI";
        case 0x10DE: return "NVIDIA";
        case 0x10EC: return "Realtek";
        case 0x15AD: return "VMware";
        case 0x1234: return "QEMU/Bochs";
        case 0x1AF4: return "Red Hat (virtio)";
        case 0x1B36: return "Red Hat (QEMU)";
        default:     return "unknown vendor";
    }
}
