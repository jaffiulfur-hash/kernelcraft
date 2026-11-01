#ifndef MM_ASPACE_H
#define MM_ASPACE_H

#include <stdint.h>

/*
 * Per-process address spaces.
 *
 * Every user process gets its own PML4 (and so its own CR3). The first
 * gigabyte of every address space is the SAME shared kernel identity map
 * (supervisor-only, so ring 3 cannot touch it); on top of that each process
 * has a private window of 4 KiB pages starting at USER_BASE.
 *
 *   virtual 0x00000000 - 0x3FFFFFFF   kernel identity map, shared, no US bit
 *   virtual 0x40000000 - 0x401FFFFF   this process's code, data and stack
 *   anything else                     unmapped: touching it is a page fault
 *
 * Two processes use the same virtual addresses but different physical
 * frames, which is the whole point.
 */

#define USER_BASE        0x40000000ull
#define USER_LIMIT       0x40200000ull     /* one page table covers 2 MiB */
#define USER_STACK_PAGES 4
#define USER_STACK_TOP   USER_LIMIT        /* the stack grows down from here */
#define ASPACE_MAX_PAGES 96

struct aspace {
    uint64_t cr3;                       /* physical address of the PML4 */
    uint64_t pdpt, pd, pt;              /* the other table frames */
    uint64_t frames[ASPACE_MAX_PAGES];  /* user pages owned by this space */
    int nframes;
};

/* Remember the boot page tables and turn on the no-execute bit. */
void aspace_init(void);
uint64_t aspace_kernel_cr3(void);
int aspace_nx_enabled(void);

struct aspace *aspace_create(void);
void aspace_destroy(struct aspace *as);

/*
 * Allocate a zeroed frame and map it at `virt` (page aligned, inside the
 * user window). Returns the frame's physical address - which, because of the
 * identity map, is also a pointer the kernel can write through - or 0.
 */
uint64_t aspace_map_new_page(struct aspace *as, uint64_t virt, int writable, int executable);

/* Is [addr, addr+len) entirely mapped for the user (and writable if asked)? */
int aspace_user_range_ok(const struct aspace *as, uint64_t addr, uint64_t len, int write);

#endif
