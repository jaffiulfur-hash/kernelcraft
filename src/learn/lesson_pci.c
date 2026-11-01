#include "learn/learn.h"
#include "arch/pci.h"
#include "lib/kprintf.h"

void pci_print_devices(void) {
    struct pci_device list[PCI_MAX_DEVICES];
    int n = pci_scan(list, PCI_MAX_DEVICES);

    kprintf("  bus:dev.fn  vendor:device  class\n");
    for (int i = 0; i < n; i++) {
        kprintf("  %02x:%02x.%x     %04x:%04x      %s (%s)\n", list[i].bus, list[i].dev,
                list[i].func, list[i].vendor, list[i].device,
                pci_class_name(list[i].class_code, list[i].subclass),
                pci_vendor_name(list[i].vendor));
    }
    kprintf("  %d function(s) found\n", n);
}

static void print_bar(int index, uint32_t bar) {
    if (bar == 0) {
        return;
    }

    if (bar & 1) {
        kprintf("  BAR%d  0x%08x  I/O ports at 0x%x\n", index, bar, bar & ~3u);
    } else {
        kprintf("  BAR%d  0x%08x  memory%s at 0x%x\n", index, bar,
                (bar & 8) ? " (prefetchable)" : "", bar & ~0xFu);
    }
}

void lesson_pci(void) {
    learn_page("Finding hardware: PCI");
    learn_heading("How does the OS know what is plugged in?");
    learn_para("Early PCs had fixed addresses for everything: the keyboard at "
               "0x60, the first serial port at 0x3F8. Modern machines have "
               "dozens of devices whose addresses are assigned at boot, so the "
               "OS must discover them. PCI is the standard way. Every device "
               "is named by bus : device . function, and each has 256 bytes "
               "of configuration space that the OS can read.");
    learn_para("Software reaches it through two ports: write an address "
               "(bus, device, function, register) to 0xCF8, then read the 32-bit "
               "answer from 0xCFC (arch/pci.c). Register 0 holds a vendor ID "
               "and a device ID; a missing device reads back 0xFFFF. So "
               "enumeration is just a loop over buses and slots, noting who "
               "answers. A driver then claims devices by ID or by class code "
               "(0x01 = storage, 0x02 = network, 0x03 = display...).");
    learn_para("The configuration space also holds Base Address Registers "
               "(BARs) that say where the device's registers live: in I/O "
               "port space or mapped into memory (MMIO).");
    LEARN_NEXT();

    learn_page("Finding hardware: PCI");
    learn_live("every PCI function on this machine (same as the 'pci' command)");
    pci_print_devices();
    kprintf("\n");
    learn_para("Under QEMU you typically see a host bridge, an ISA/IDE/ACPI "
               "bridge, a VGA adapter and a network card - all Intel or QEMU "
               "inventions. On real hardware the list is longer: storage, USB, "
               "audio, graphics, and bridges leading to more buses.");
    learn_try("pci   (the same list from the shell)");
    LEARN_NEXT();

    struct pci_device list[PCI_MAX_DEVICES];
    int n = pci_scan(list, PCI_MAX_DEVICES);
    int pick = 0;

    for (int i = 0; i < n; i++) {
        if (list[i].class_code == 0x03) {       /* prefer the display adapter */
            pick = i;
            break;
        }
    }

    learn_page("Finding hardware: PCI");
    learn_live("reading one device's configuration header");
    if (n == 0) {
        kprintf("  no PCI devices found\n");
        return;
    }

    const struct pci_device *d = &list[pick];
    uint32_t cmd = pci_config_read32(d->bus, d->dev, d->func, 0x04);

    kprintf("  device %02x:%02x.%x  %04x:%04x  %s\n", d->bus, d->dev, d->func, d->vendor,
            d->device, pci_class_name(d->class_code, d->subclass));
    kprintf("  class %02x  subclass %02x  prog-if %02x  header type %02x\n", d->class_code,
            d->subclass, d->prog_if, d->header_type & 0x7F);
    kprintf("  command 0x%04x:%s%s%s\n", cmd & 0xFFFF, (cmd & 1) ? " io-space" : "",
            (cmd & 2) ? " memory-space" : "", (cmd & 4) ? " bus-master" : "");
    if ((d->header_type & 0x7F) == 0) {
        for (int bar = 0; bar < 6; bar++) {
            print_bar(bar, pci_config_read32(d->bus, d->dev, d->func, (uint8_t)(0x10 + 4 * bar)));
        }
    }
    kprintf("\n");
    learn_para("The command register is the driver's switch: setting memory-"
               "space lets the device answer memory reads; bus-master lets it "
               "start DMA and write to RAM on its own. A BAR with bit 0 set "
               "is an I/O port range; otherwise it is a memory range, and "
               "bits 1-2 say whether the address is 32 or 64 bits wide. This "
               "lesson only reads; a real driver would size the BARs by "
               "writing all ones and reading back.");
    learn_try("exercise: find the class 0x02 (network) device and read the first "
              "32-bit register through its memory BAR - the first step of a NIC "
              "driver (the e1000 keeps its MAC address in its receive-address "
              "registers).");
}
