#include "learn/learn.h"
#include "lib/kprintf.h"
#include "mm/aspace.h"
#include "mm/paging.h"
#include "proc/elf.h"
#include "proc/process.h"

static const char *const level_names[4] = { "PML4", "PDPT", "PD  ", "PT  " };

/* All four levels must carry the US bit for ring 3 to reach a page. */
static int user_reachable(const struct paging_walk *w) {
    for (int i = 0; i < w->levels_read; i++) {
        if (!(w->entry[i] & PTE_USER)) {
            return 0;
        }
    }

    return w->mapped;
}

static void show_walk(const char *label, uint64_t cr3, uint64_t virt) {
    struct paging_walk w;
    char flags[40];

    paging_walk_cr3(cr3, virt, &w);
    kprintf("  %s  CR3=0x%lx  virtual 0x%lx\n", label, cr3, virt);
    for (int i = 0; i < w.levels_read; i++) {
        paging_format_flags(w.entry[i], flags, sizeof(flags));
        kprintf("    %s[%3u] 0x%016lx  %s\n", level_names[i], w.index[i], w.entry[i],
                (w.entry[i] & PTE_PRESENT) ? flags : "NOT PRESENT");
    }
    if (w.mapped) {
        kprintf("    -> physical 0x%lx, ring 3 %s\n", w.phys,
                user_reachable(&w) ? "CAN access it" : "is BLOCKED (a US bit is missing)");
    } else {
        kprintf("    -> no translation: any access is a page fault\n");
    }
}

/* Virtual address of the writable segment of a program (where its globals live). */
static uint64_t data_address(const char *name) {
    const struct user_program *p = process_find_program(name);

    if (!p || elf_check(p->start, (size_t)(p->end - p->start))) {
        return 0;
    }

    const struct elf64_ehdr *h = (const struct elf64_ehdr *)p->start;

    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *ph =
            (const struct elf64_phdr *)(p->start + h->phoff + (uint64_t)i * h->phentsize);

        if (ph->type == ELF_PT_LOAD && (ph->flags & ELF_PF_W) && ph->memsz) {
            return ph->vaddr;
        }
    }

    return 0;
}

void lesson_aspace(void) {
    learn_page("Address spaces and isolation");
    learn_heading("Every process gets its own view of memory");
    learn_para("An address space is the set of virtual addresses a program can "
               "use, and which memory each one reaches. Give every process its "
               "own top-level page table (its own CR3) and two programs can "
               "use the same addresses without ever touching each other's "
               "data. The kernel does it like this:");
    kprintf("  virtual 0x00000000 - 0x3FFFFFFF   kernel identity map: the SAME in\n");
    kprintf("                                    every space, but no U bit\n");
    kprintf("  virtual 0x40000000 - 0x401FFFFF   this process: code r-x, rodata r--,\n");
    kprintf("                                    data rw-, stack rw- (NX on data)\n");
    kprintf("  everything else                   not mapped\n\n");
    learn_para("Sharing the kernel's gigabyte means a trap into the kernel does "
               "not have to switch page tables. The rule that keeps ring 3 out "
               "of it: a page is user-accessible only if the U/S bit is set at "
               "EVERY level of the walk. The kernel's entries lack it at the "
               "page-directory level, so the whole gigabyte is closed to "
               "ring 3 even though it sits in the same tables.");
    LEARN_NEXT();

    learn_page("Address spaces and isolation");
    learn_live("two copies of user/prog_isolate.c, same virtual address");
    const char *why = 0;
    struct thread *a = process_spawn("isolate", 0, &why);
    struct thread *b = process_spawn("isolate", 0, &why);
    uint64_t va = data_address("isolate");

    if (!a || !b || !va) {
        kprintf("  could not start the demo processes\n");
        if (a) process_wait(a, 0);
        if (b) process_wait(b, 0);
        return;
    }

    thread_sleep_ticks(15);            /* let both write their value, then sleep */
    show_walk("process A", a->as->cr3, va);
    show_walk("process B", b->as->cr3, va);
    process_wait(a, 0);
    process_wait(b, 0);
    kprintf("\n");
    learn_para("Same virtual address, different CR3, different physical frame. "
               "Each wrote its own number there and each still reads it back: "
               "the writes could not collide because they landed in different "
               "RAM.");
    LEARN_NEXT();

    learn_page("Address spaces and isolation");
    learn_live("what ring 3 can and cannot reach, through a fresh address space");
    struct aspace *as = aspace_create();

    if (!as) {
        kprintf("  out of memory\n");
        return;
    }
    aspace_map_new_page(as, USER_BASE, 1, 0);
    show_walk("user page   ", as->cr3, USER_BASE);
    show_walk("kernel image", as->cr3, 0x100000);
    show_walk("unmapped    ", as->cr3, 0x80000000ull);
    aspace_destroy(as);
    kprintf("\n");
    learn_para("The kernel image IS in this address space (the kernel needs "
               "it when ring 3 traps in) but the page-directory entry has no "
               "US bit, so ring 3 is blocked. The data page also carries NX: "
               "it can be read and written but never executed.");
    LEARN_NEXT();

    learn_page("Address spaces and isolation");
    learn_heading("What a real kernel adds");
    kprintf("  demand paging     map pages when first touched, from the fault handler\n");
    kprintf("  copy-on-write     fork() shares pages until one side writes\n");
    kprintf("  shared memory     the same frame mapped into two spaces on purpose\n");
    kprintf("  ASLR              randomise where code and stack are placed\n");
    kprintf("  SMEP / SMAP       stop the kernel executing or touching user pages by\n");
    kprintf("                    accident - our syscalls validate every pointer instead\n");
    kprintf("  PCID              tag TLB entries per space, so CR3 writes are cheaper\n\n");
    learn_para("Each time the scheduler switches processes it loads a new CR3, "
               "which flushes the TLB: the cache of translations. That is one "
               "reason a context switch between processes costs more than "
               "between threads of the same process.");
    learn_try("run isolate isolate isolate - then 'learn sched' and compare the "
              "cost of switching.");
}
