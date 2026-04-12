#include "kernel.h"
#include "vga.h"
#include "arch/gdt.h"
#include "arch/interrupts.h"
#include "arch/keyboard.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "boot/multiboot2.h"
#include "mm/pmm.h"
#include "shell.h"

void kmain(uint32_t mb_magic, uint32_t mb_info_addr) {
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    log_init();

    log_write("KernelCraft OS\n");
    log_write("stage 4.0: physical memory manager\n");

    if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        log_write("invalid multiboot2 magic\n");
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    gdt_init();
    log_write("gdt loaded\n");

    interrupts_init();
    log_write("interrupts loaded\n");

    pit_init(100);
    keyboard_init();
    shell_init();

    log_write("pit started at 100 Hz\n");
    log_write("keyboard ready\n");

    pmm_init(mb2_find_mmap_tag(mb_info_addr));

    shell_prompt();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
