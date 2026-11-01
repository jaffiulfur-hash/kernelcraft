#include "kernel.h"
#include "vga.h"
#include "arch/gdt.h"
#include "arch/interrupts.h"
#include "arch/keyboard.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "arch/power.h"
#include "arch/qemu.h"
#include "boot/bootinfo.h"
#include "boot/multiboot2.h"
#include "lib/kprintf.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "selftest.h"
#include "shell.h"

void kmain(uint32_t mb_magic, uint32_t mb_info_addr) {
    vga_init();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    log_init();
    bootinfo_init(mb_magic, mb_info_addr);

    kprintf("%s\n", KERNELCRAFT_NAME);
    kprintf("stage %s: %s\n", KERNELCRAFT_STAGE, KERNELCRAFT_STAGE_TITLE);

    if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprintf("invalid multiboot2 magic: 0x%08x\n", mb_magic);
        arch_halt();
    }

    const char *loader = mb2_bootloader_name(mb_info_addr);
    if (loader) {
        kprintf("bootloader: %s\n", loader);
    }

    gdt_init();
    log_write("gdt loaded\n");

    interrupts_init();
    log_write("interrupts loaded\n");

    pit_init(PIT_HZ);
    keyboard_init();
    shell_init();

    kprintf("pit started at %d Hz\n", PIT_HZ);
    log_write("keyboard ready\n");

    pmm_init(mb2_find_mmap_tag(mb_info_addr));

    /* The boot info block (and the mmap inside it) lives in ordinary RAM. */
    pmm_reserve_range(mb_info_addr, mb2_info_size(mb_info_addr));
    pmm_print_summary();

    heap_init();

    selftest_init(mb_info_addr);

#ifdef KC_SELFTEST
    /* `make test`: run the suite headless and report through QEMU's exit code. */
    int failed = selftest_run();
    qemu_exit(failed ? QEMU_EXIT_FAILURE : QEMU_EXIT_SUCCESS);
    kprintf("isa-debug-exit device not present; continuing to shell\n");
#endif

    shell_prompt();

    /*
     * The keyboard interrupt only queues characters. All the real work - the
     * shell, lessons, anything that blocks - runs here, in ordinary context
     * with interrupts enabled, and sleeps in keyboard_getc() when idle.
     */
    for (;;) {
        shell_handle_char(keyboard_getc());
    }
}
