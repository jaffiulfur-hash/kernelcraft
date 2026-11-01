#include "arch/power.h"
#include "arch/io.h"

void arch_halt(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void arch_reboot(void) {
    struct __attribute__((packed)) {
        uint16_t limit;
        uint64_t base;
    } empty_idt = { 0, 0 };

    __asm__ volatile ("cli");

    /* Wait for the 8042 input buffer to drain, then pulse the CPU reset line. */
    for (int i = 0; i < 0x10000 && (inb(0x64) & 0x02); i++) {
    }
    outb(0x64, 0xFE);

    /* Still here: load an empty IDT and raise an exception to triple fault. */
    __asm__ volatile ("lidt %0; int3" : : "m"(empty_idt));

    arch_halt();
}
