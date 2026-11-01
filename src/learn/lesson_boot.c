#include "learn/learn.h"
#include "arch/cpu.h"
#include "boot/bootinfo.h"
#include "boot/multiboot2.h"
#include "layout.h"
#include "lib/kprintf.h"

static void section_line(const char *name, const char *start, const char *end) {
    uint64_t s = (uint64_t)(uintptr_t)start;
    uint64_t e = (uint64_t)(uintptr_t)end;

    kprintf("  %-10s 0x%08lx - 0x%08lx  %6lu bytes\n", name, s, e, e - s);
}

void lesson_boot(void) {
    learn_page("From power button to kmain");
    learn_heading("Firmware, bootloader, kernel");
    learn_para("At power-on the CPU starts in 16-bit real mode and runs firmware "
               "(BIOS or UEFI) from ROM. The firmware tests the hardware, finds "
               "a boot device and loads a small program from it. Here that "
               "program is GRUB. GRUB reads grub.cfg, sees 'multiboot2 "
               "/boot/kernel.elf', and loads our ELF file at 1 MiB, exactly "
               "where linker.ld says it lives.");
    learn_para("How does GRUB know the file is bootable? It searches the first "
               "32 KiB for a Multiboot2 header: the magic 0xE85250D6, an "
               "architecture, a length, and a checksum chosen so the four "
               "numbers add up to zero. boot.asm emits that header in its own "
               ".multiboot section and linker.ld places the section first.");
    learn_para("GRUB then enters 32-bit protected mode with paging and "
               "interrupts off, leaves a boot-information block in memory, puts "
               "the magic 0x36D76289 in EAX and the block's address in EBX, and "
               "jumps to _start.");
    LEARN_NEXT();

    learn_page("From power button to kmain");
    learn_live("what GRUB handed to this kernel");
    uint32_t addr = bootinfo_addr();

    kprintf("  magic (EAX)   0x%08x  %s\n", bootinfo_magic(),
            bootinfo_magic() == MULTIBOOT2_BOOTLOADER_MAGIC ? "= Multiboot2, good" : "UNEXPECTED");
    kprintf("  info (EBX)    0x%08x  (%u bytes)\n", addr, mb2_info_size(addr));
    kprintf("  bootloader    %s\n\n", mb2_bootloader_name(addr) ? mb2_bootloader_name(addr) : "(none)");
    kprintf("  tags in the info block:\n");
    for (const struct multiboot_tag *t = mb2_first_tag(addr); t; t = mb2_next_tag(addr, t)) {
        kprintf("    type %-2u %-20s %u bytes\n", t->type, mb2_tag_type_name(t->type), t->size);
    }
    kprintf("\n");
    learn_para("Each tag is {type, size, data...}, padded to 8 bytes. The memory "
               "map tag (type 6) is how the kernel learns which RAM is usable - "
               "try 'mmap'. Read src/boot/multiboot2.c to see the walk.");
    LEARN_NEXT();

    learn_page("From power button to kmain");
    learn_heading("boot.asm: 32-bit to 64-bit in nine steps");
    kprintf("  1. cli, save EAX/EBX (the bootloader's hand-off) in memory\n");
    kprintf("  2. point ESP at a 16 KiB stack in .bss\n");
    kprintf("  3. build page tables PML4 -> PDPT -> PD with 512 x 2 MiB pages:\n");
    kprintf("     the first 1 GiB identity-mapped (virtual == physical)\n");
    kprintf("  4. lgdt: a temporary GDT with a 64-bit code segment\n");
    kprintf("  5. CR4.PAE = 1   64-bit paging needs physical address extension\n");
    kprintf("  6. CR3 = PML4    tell the MMU where the tables are\n");
    kprintf("  7. EFER.LME = 1  model-specific register 0xC0000080: allow long mode\n");
    kprintf("  8. CR0.PG = 1    paging on; the CPU is now in 32-bit compatibility mode\n");
    kprintf("  9. far jump to the 64-bit code segment: long mode! call kmain\n\n");
    learn_para("Paging is mandatory for long mode, which is why the page tables "
               "come first. The identity map keeps every address meaning what it "
               "meant a moment earlier, so execution continues seamlessly.");
    LEARN_NEXT();

    learn_page("From power button to kmain");
    learn_live("proof that we are in long mode right now");
    uint64_t cr0 = read_cr0(), cr4 = read_cr4(), efer = rdmsr(MSR_EFER);

    kprintf("  CR0.PE  = %d   protected mode on\n", (int)(cr0 & 1));
    kprintf("  CR0.PG  = %d   paging on\n", (int)((cr0 >> 31) & 1));
    kprintf("  CR4.PAE = %d   physical address extension on\n", (int)((cr4 >> 5) & 1));
    kprintf("  EFER.LME= %d   long mode enabled (we asked for it)\n", (int)((efer >> 8) & 1));
    kprintf("  EFER.LMA= %d   long mode active (the CPU confirms it)\n", (int)((efer >> 10) & 1));
    kprintf("  CS      = 0x%02x  privilege level (CPL) = %d\n\n", read_cs(), read_cs() & 3);
    learn_heading("The kernel image in memory (from linker.ld symbols)");
    section_line(".multiboot", __kernel_start, __multiboot_end);
    section_line(".text", __text_start, __text_end);
    section_line(".rodata", __rodata_start, __rodata_end);
    section_line(".data", __data_start, __data_end);
    section_line(".bss", __bss_start, __bss_end);
    kprintf("\n");
    learn_para(".bss holds the boot stack, the page tables and the PMM bitmaps - "
               "memory the file does not need to store, only to reserve.");
    LEARN_NEXT();

    learn_page("From power button to kmain");
    learn_heading("kmain: the order things start in");
    kprintf("  vga_init, log_init      so we can print (VGA + serial)\n");
    kprintf("  gdt_init                our own segment table replaces boot.asm's\n");
    kprintf("  interrupts_init         IDT, remap the PIC, then enable interrupts\n");
    kprintf("  pit_init, keyboard      a clock and an input device\n");
    kprintf("  pmm_init + reserve      learn what RAM exists, protect our own\n");
    kprintf("  heap_init               kmalloc() works from here on\n");
    kprintf("  shell loop              wait for a key, run a command, repeat\n\n");
    learn_para("Order matters: interrupts come before anything risky so faults "
               "are reported instead of silently rebooting, and memory "
               "management comes before anything that needs to allocate.");
    learn_try("open src/kmain.c and move heap_init() above pmm_init() - what "
              "happens, and why?");
}
