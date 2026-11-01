#include "learn/learn.h"
#include "arch/cpu.h"
#include "layout.h"
#include "lib/kprintf.h"
#include "mm/heap.h"
#include "mm/paging.h"

static void one_line(const char *what, uint64_t virt) {
    struct paging_walk w;
    char flags[40];

    paging_walk(virt, &w);
    kprintf("  %-16s 0x%016lx -> ", what, virt);

    if (!w.canonical) {
        kprintf("not canonical (#GP)\n");
    } else if (w.mapped) {
        paging_format_flags(w.entry[w.levels_read - 1], flags, sizeof(flags));
        kprintf("0x%08lx  %s page  [%s]\n", w.phys,
                w.page_size == (1ull << 21) ? "2 MiB" : w.page_size == 4096 ? "4 KiB" : "1 GiB",
                flags);
    } else {
        kprintf("NOT MAPPED (stopped at %s level)\n",
                w.fail_level == 0 ? "PML4" : w.fail_level == 1 ? "PDPT" :
                w.fail_level == 2 ? "PD" : "PT");
    }
}

void lesson_paging(void) {
    learn_page("Virtual memory and page tables");
    learn_heading("Addresses programs use are not addresses RAM has");
    learn_para("With paging on, every address the CPU sees is virtual. The "
               "MMU translates it to a physical address by walking a tree of "
               "tables in memory. On x86_64 there are four levels, each table "
               "4 KiB holding 512 eight-byte entries, so each level consumes "
               "9 bits of the address:");
    kprintf("   63      48 47    39 38    30 29    21 20    12 11         0\n");
    kprintf("  [ sign ext.][ PML4 ][ PDPT ][  PD  ][  PT  ][  offset  ]\n");
    kprintf("                 idx     idx     idx     idx    12 bits\n\n");
    learn_para("CR3 holds the physical address of the PML4. Each entry holds "
               "the address of the next table plus flag bits: P (present), RW "
               "(writable), US (user may access), A and D (accessed, dirty - "
               "set by hardware), PS (this entry maps a big page and the walk "
               "ends early) and NX (no execute). A miss, or a permission "
               "violation, raises a page fault (vector 14) and CR2 holds the "
               "address. Results are cached in the TLB.");
    LEARN_NEXT();

    struct paging_walk w;
    void *probe = kmalloc(16);
    uint64_t heap_addr = (uint64_t)(uintptr_t)probe;

    learn_page("Virtual memory and page tables");
    learn_live("walking the page tables for a kernel heap address");
    paging_walk(heap_addr, &w);
    paging_print_walk(&w);
    kfree(probe);
    kprintf("\n");
    learn_para("boot.asm made this: PML4[0] -> PDPT[0] -> PD, whose 512 entries "
               "are all 2 MiB pages (PS set, flags 0x83), so the walk stops at "
               "the PD level and the low 21 bits are the offset. Physical == "
               "virtual: an identity map. That is why the kernel can use a "
               "pointer and a physical address interchangeably.");
    learn_try("vtop 0xB8000    vtop 0x100000    vtop <any address you like>");
    LEARN_NEXT();

    learn_page("Virtual memory and page tables");
    learn_live("translating a few addresses");
    one_line("kernel image", KERNEL_START);
    one_line("VGA text buffer", 0xB8000);
    one_line("heap", heap_addr);
    one_line("last mapped byte", BOOT_IDENTITY_MAP_BYTES - 1);
    one_line("one byte past", BOOT_IDENTITY_MAP_BYTES);
    one_line("high half", 0xFFFFFFFF80000000ull);
    one_line("non-canonical", 0x0001000000000000ull);
    kprintf("\n");
    learn_para("Anything at or above 1 GiB has no entry: touching it raises a "
               "page fault. Try 'pf' at the shell - the kernel decodes CR2 and "
               "the error code (not-present, write/read, user/supervisor) "
               "before halting. A real kernel's fault handler would map a "
               "page and return; ours just reports.");
    learn_para("Notice every flag shown has RW and no NX: all memory is writable "
               "and executable, even .text and .rodata, because 2 MiB pages "
               "cannot give different sections different permissions.");
    LEARN_NEXT();

    learn_page("Virtual memory and page tables");
    learn_heading("What a real kernel does with this machinery");
    kprintf("  per-process CR3     each process gets its own page tables, so its\n");
    kprintf("                      address 0x400000 is different RAM from yours\n");
    kprintf("  4 KiB pages         per-section permissions: .text r-x, .rodata r--,\n");
    kprintf("                      data rw- with NX (W^X)\n");
    kprintf("  user bit            ring 3 may touch only pages with US set, so a\n");
    kprintf("                      program cannot read or write kernel memory\n");
    kprintf("  demand paging      map pages lazily from the fault handler\n");
    kprintf("  copy-on-write       fork() shares pages until someone writes\n");
    kprintf("  memory-mapped files the page cache and mmap() are page tables too\n");
    kprintf("  swapping            a not-present entry can point at disk\n\n");
    learn_para("Everything above is the same four-level walk you just watched, "
               "plus policy. KernelCraft now does the first three for user "
               "processes - see 'learn aspace'. The mechanism is small; the "
               "policy is what fills the textbooks.");
    learn_try("exercise: write paging_map(virt, phys, flags) that builds a 4 KiB "
              "mapping, then map the VGA buffer at another address.");
}
