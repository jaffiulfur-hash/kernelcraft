#include "learn/learn.h"
#include "arch/cpu.h"
#include "lib/kprintf.h"

static void decode_entry(int index, uint64_t raw) {
    uint8_t access = (uint8_t)(raw >> 40);
    uint8_t flags = (uint8_t)((raw >> 52) & 0xF);

    if (!(access & 0x80)) {
        kprintf("  %d   0x%02x  0x%016lx  null descriptor (present bit clear)\n",
                index, index * 8, raw);
        return;
    }

    const char *kind;
    const char *rw;

    if (!(access & 0x10)) {
        kind = ((access & 0xF) == 0x9 || (access & 0xF) == 0xB) ? "TSS" : "system";
        rw = "";
    } else if (access & 0x08) {
        kind = "code";
        rw = (access & 0x02) ? "readable" : "execute-only";
    } else {
        kind = "data";
        rw = (access & 0x02) ? "writable" : "read-only";
    }

    kprintf("  %d   0x%02x  0x%016lx  %-6s DPL=%d %-9s L=%d D=%d G=%d\n", index,
            index * 8, raw, kind, (access >> 5) & 3, rw, (flags >> 1) & 1,
            (flags >> 2) & 1, (flags >> 3) & 1);
}

void lesson_gdt(void) {
    learn_page("Segmentation and the GDT");
    learn_heading("A feature from 1982 that will not go away");
    learn_para("In 16/32-bit mode every memory access went through a segment: "
               "a base address and a limit chosen by a selector in a segment "
               "register, looked up in the Global Descriptor Table. In long "
               "mode the CPU ignores base and limit for code and data - memory "
               "is flat - but the GDT is still required. It tells the CPU which "
               "privilege ring a segment belongs to (DPL) and whether code is "
               "64-bit (the L bit), and the CPU consults it on every ring change.");
    learn_para("A selector is 16 bits: index (13 bits), table indicator (1), "
               "requested privilege level (2). Selector 0x08 means index 1, "
               "ring 0. A descriptor is 8 bytes:");
    kprintf("  bits 63-56  55-52  51-48   47-40   39-16   15-0\n");
    kprintf("  base[31:24] flags  limit   access  base    limit[15:0]\n");
    kprintf("                     [19:16]         [23:0]\n\n");
    learn_para("access: P (present), DPL (2 bits), S (1 = code/data), then the "
               "type bits. flags: G (4 KiB granularity), D (32-bit), L (64-bit "
               "code).");
    LEARN_NEXT();

    struct dtr gdtr;

    sgdt(&gdtr);

    learn_page("Segmentation and the GDT");
    learn_live("the GDT the CPU is using right now (sgdt)");
    kprintf("  GDTR base  = 0x%016lx\n", gdtr.base);
    kprintf("  GDTR limit = %u  (%u descriptors)\n\n", gdtr.limit, (gdtr.limit + 1u) / 8u);
    kprintf("  idx sel   raw descriptor     decoded\n");

    const uint64_t *table = (const uint64_t *)(uintptr_t)gdtr.base;
    int count = (gdtr.limit + 1) / 8;

    for (int i = 0; i < count && i < 8; i++) {
        if (i == 6) {
            kprintf("  6   --    0x%016lx  upper half of the 16-byte TSS descriptor\n", table[i]);
        } else {
            decode_entry(i, table[i]);
        }
    }

    kprintf("\n");
    learn_para("Entry 1 is kernel code: executable, ring 0, L=1 so the CPU runs "
               "it as 64-bit. Entry 2 is kernel data. Entries 3 and 4 are the "
               "same for ring 3 (DPL=3), and 5-6 hold the TSS. Base and limit "
               "are zero for segments: they no longer matter. All built by "
               "gdt_init() in gdt.c.");
    LEARN_NEXT();

    learn_page("Segmentation and the GDT");
    learn_heading("Rings, selectors and the TSS");
    kprintf("  idx  selector  purpose\n");
    kprintf("   0   0x00      null (required, never usable)\n");
    kprintf("   1   0x08      kernel code, ring 0\n");
    kprintf("   2   0x10      kernel data, ring 0\n");
    kprintf("   3   0x1B      user data, ring 3          (0x18 | RPL 3)\n");
    kprintf("   4   0x23      user code, ring 3          (0x20 | RPL 3)\n");
    kprintf("  5-6  0x28      TSS, a 16-byte system descriptor\n\n");
    learn_para("The user entries let programs run in ring 3. The TSS (task state "
               "segment) holds rsp0, the stack the CPU switches to when a ring "
               "3 program traps into the kernel; without it a system call "
               "would run on the user's own stack, which the program controls. "
               "The 'usermode' lesson runs a real program through these.");
    learn_para("Spot the detail: entry 2's access byte reads 0x93 although "
               "gdt.c wrote 0x92. The CPU set the 'accessed' bit itself when "
               "the descriptor was loaded into DS and SS.");
    learn_try("in gdt.c change the user code access byte 0xFA to 0x9A (DPL 0), "
              "then 'run hello' and see what the CPU does.");
}
