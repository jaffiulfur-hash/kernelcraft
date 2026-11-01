#ifndef MM_PAGING_H
#define MM_PAGING_H

#include <stdint.h>

/*
 * Read-only view of the x86_64 page tables the CPU is using (CR3).
 *
 * This does not create or change mappings - boot.asm built the only ones
 * (an identity map of the first 1 GiB) - but it walks them exactly the way
 * the MMU does, which is the whole point of the paging lesson.
 */

#define PTE_PRESENT  (1ull << 0)
#define PTE_WRITABLE (1ull << 1)
#define PTE_USER     (1ull << 2)
#define PTE_ACCESSED (1ull << 5)
#define PTE_DIRTY    (1ull << 6)
#define PTE_HUGE     (1ull << 7)     /* PS: this entry maps a big page */
#define PTE_GLOBAL   (1ull << 8)
#define PTE_NX       (1ull << 63)

struct paging_walk {
    uint64_t virt;
    uint64_t cr3;
    int canonical;            /* bits 63:48 must copy bit 47 or the CPU faults */
    int levels_read;          /* how many table levels were actually read */
    uint16_t index[4];        /* PML4, PDPT, PD, PT indices of `virt` */
    uint64_t entry[4];        /* the raw entries that were read */
    int mapped;               /* translation succeeded */
    int fail_level;           /* level (0-3) whose entry was not present */
    uint64_t phys;            /* valid if mapped */
    uint64_t page_size;       /* 4 KiB, 2 MiB or 1 GiB, if mapped */
};

/* Returns 0 if `virt` is mapped, -1 otherwise. `*w` is filled either way. */
int paging_walk(uint64_t virt, struct paging_walk *w);

/* Print the level-by-level translation (used by `vtop` and the lesson). */
void paging_print_walk(const struct paging_walk *w);

/* "P RW US ..." for the flag bits of a page-table entry. */
void paging_format_flags(uint64_t entry, char *buf, unsigned size);

#endif
