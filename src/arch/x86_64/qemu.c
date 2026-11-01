#include "arch/qemu.h"
#include "arch/io.h"

#define QEMU_DEBUG_EXIT_PORT 0xF4

void qemu_exit(uint8_t code) {
    outb(QEMU_DEBUG_EXIT_PORT, code);
}
