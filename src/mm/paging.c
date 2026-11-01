#include "mm/paging.h"
#include "arch/cpu.h"
#include "layout.h"
#include "lib/kprintf.h"
#include "lib/string.h"

#define ENTRY_ADDR_MASK ((uint64_t)0x000FFFFFFFFFF000ull)

static const char *const level_names[4] = { "PML4", "PDPT", "PD  ", "PT  " };
static const unsigned level_shift[4] = { 39, 30, 21, 12 };

void paging_format_flags(uint64_t e, char *buf, unsigned size) {
    static const struct { uint64_t bit; const char *name; } names[] = {
        { PTE_PRESENT, "P" }, { PTE_WRITABLE, "RW" }, { PTE_USER, "US" },
        { PTE_ACCESSED, "A" }, { PTE_DIRTY, "D" }, { PTE_HUGE, "PS" },
        { PTE_GLOBAL, "G" }, { PTE_NX, "NX" },
    };
    unsigned pos = 0;

    buf[0] = '\0';
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (e & names[i].bit) {
            pos += (unsigned)ksnprintf(buf + pos, size - pos, "%s%s",
                                       pos ? " " : "", names[i].name);
            if (pos >= size) {
                return;
            }
        }
    }
}

int paging_walk(uint64_t virt, struct paging_walk *w) {
    return paging_walk_cr3(read_cr3(), virt, w);
}

int paging_walk_cr3(uint64_t cr3, uint64_t virt, struct paging_walk *w) {
    memset(w, 0, sizeof(*w));
    w->virt = virt;
    w->cr3 = cr3;
    w->fail_level = -1;

    /* Bits 63:48 must be copies of bit 47 ("canonical" addresses). */
    uint64_t top = virt >> 47;
    w->canonical = (top == 0 || top == 0x1FFFF);
    if (!w->canonical) {
        return -1;
    }

    uint64_t table = w->cr3 & ENTRY_ADDR_MASK;

    for (int level = 0; level < 4; level++) {
        /* Tables are reached through the identity map, so they must lie in it. */
        if (table >= BOOT_IDENTITY_MAP_BYTES) {
            w->fail_level = level;
            return -1;
        }

        w->index[level] = (uint16_t)((virt >> level_shift[level]) & 0x1FF);
        uint64_t entry = ((const uint64_t *)(uintptr_t)table)[w->index[level]];

        w->entry[level] = entry;
        w->levels_read = level + 1;

        if (!(entry & PTE_PRESENT)) {
            w->fail_level = level;
            return -1;
        }

        /* PS at the PDPT or PD level ends the walk with a 1 GiB / 2 MiB page. */
        if ((level == 1 || level == 2) && (entry & PTE_HUGE)) {
            w->page_size = level == 1 ? (1ull << 30) : (1ull << 21);
            w->phys = (entry & ENTRY_ADDR_MASK & ~(w->page_size - 1)) |
                      (virt & (w->page_size - 1));
            w->mapped = 1;
            return 0;
        }

        if (level == 3) {
            w->page_size = 4096;
            w->phys = (entry & ENTRY_ADDR_MASK) | (virt & 0xFFF);
            w->mapped = 1;
            return 0;
        }

        table = entry & ENTRY_ADDR_MASK;
    }

    return -1;
}

void paging_print_walk(const struct paging_walk *w) {
    char flags[40];

    kprintf("virtual address 0x%016lx\n", w->virt);

    if (!w->canonical) {
        kprintf("  not canonical: bits 63:48 differ from bit 47 -> #GP, not a page fault\n");
        return;
    }

    kprintf("  CR3 = 0x%016lx (top-level table)\n", w->cr3);

    for (int level = 0; level < w->levels_read; level++) {
        paging_format_flags(w->entry[level], flags, sizeof(flags));
        kprintf("  %s[%3u] = 0x%016lx  %-14s", level_names[level], w->index[level],
                w->entry[level], flags);

        if (!(w->entry[level] & PTE_PRESENT)) {
            kprintf(" NOT PRESENT -> page fault\n");
        } else if (level < 3 && !(w->entry[level] & PTE_HUGE)) {
            kprintf(" -> next table at 0x%lx\n", w->entry[level] & ENTRY_ADDR_MASK);
        } else if (w->mapped && level == w->levels_read - 1) {
            kprintf(" -> %s page\n", w->page_size == (1ull << 30) ? "1 GiB" :
                                      w->page_size == (1ull << 21) ? "2 MiB" : "4 KiB");
        } else {
            kprintf("\n");
        }
    }

    if (w->mapped) {
        kprintf("  offset within the page: 0x%lx (low %d bits)\n",
                w->virt & (w->page_size - 1),
                w->page_size == (1ull << 30) ? 30 : w->page_size == (1ull << 21) ? 21 : 12);
        kprintf("  physical address       0x%016lx\n", w->phys);
    } else {
        kprintf("  no translation\n");
    }
}
