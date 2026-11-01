#ifndef BOOT_BOOTINFO_H
#define BOOT_BOOTINFO_H

#include <stdint.h>

/* What the bootloader handed to kmain (EAX and EBX at entry), kept for later. */
void bootinfo_init(uint32_t magic, uint32_t info_addr);
uint32_t bootinfo_magic(void);
uint32_t bootinfo_addr(void);

#endif
