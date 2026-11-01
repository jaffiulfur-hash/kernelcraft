#include "mm/aspace.h"
#include "arch/cpu.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "mm/pmm.h"

#define ENTRY_ADDR_MASK ((uint64_t)0x000FFFFFFFFFF000ull)
#define TABLE_ENTRIES   512

static uint64_t boot_cr3;
static int nx_enabled;

void aspace_init(void) {
    uint32_t a, b, c, d;

    boot_cr3 = read_cr3();

    /* CPUID 0x80000001, EDX bit 20: the CPU supports no-execute pages. */
    cpuid(0x80000001u, 0, &a, &b, &c, &d);
    if (d & (1u << 20)) {
        wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_NXE);
        nx_enabled = 1;
    }
}

uint64_t aspace_kernel_cr3(void) {
    return boot_cr3;
}

int aspace_nx_enabled(void) {
    return nx_enabled;
}

static uint64_t alloc_table(void) {
    uint64_t frame = pmm_alloc_frame();

    if (frame) {
        memset((void *)(uintptr_t)frame, 0, PMM_FRAME_SIZE);
    }

    return frame;
}

struct aspace *aspace_create(void) {
    struct aspace *as = (struct aspace *)kmalloc(sizeof(*as));

    if (!as) {
        return 0;
    }

    memset(as, 0, sizeof(*as));
    as->cr3 = alloc_table();
    as->pdpt = alloc_table();
    as->pd = alloc_table();
    as->pt = alloc_table();

    if (!as->cr3 || !as->pdpt || !as->pd || !as->pt) {
        aspace_destroy(as);
        return 0;
    }

    const uint64_t *boot_pml4 = (const uint64_t *)(uintptr_t)(boot_cr3 & ENTRY_ADDR_MASK);
    const uint64_t *boot_pdpt = (const uint64_t *)(uintptr_t)(boot_pml4[0] & ENTRY_ADDR_MASK);
    uint64_t *pml4 = (uint64_t *)(uintptr_t)as->cr3;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)as->pdpt;
    uint64_t *pd = (uint64_t *)(uintptr_t)as->pd;

    /*
     * The US bit must be set at EVERY level for ring 3 to get through, so the
     * path to the user window is user-accessible all the way down...
     */
    pml4[0] = as->pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    pdpt[1] = as->pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;      /* 1 GiB .. 2 GiB */
    pd[0] = as->pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;        /* first 2 MiB of it */

    /* ...while the kernel's gigabyte is copied from the boot tables with its
     * original supervisor-only entry: shared, but closed to ring 3. */
    pdpt[0] = boot_pdpt[0];

    return as;
}

uint64_t aspace_map_new_page(struct aspace *as, uint64_t virt, int writable, int executable) {
    if ((virt & 0xFFF) || virt < USER_BASE || virt >= USER_LIMIT ||
        as->nframes >= ASPACE_MAX_PAGES) {
        return 0;
    }

    uint64_t *pt = (uint64_t *)(uintptr_t)as->pt;
    uint64_t index = (virt - USER_BASE) >> 12;

    if (pt[index] & PTE_PRESENT) {
        return 0;                       /* already mapped */
    }

    uint64_t frame = alloc_table();     /* a zeroed frame */

    if (!frame) {
        return 0;
    }

    as->frames[as->nframes++] = frame;
    pt[index] = frame | PTE_PRESENT | PTE_USER | (writable ? PTE_WRITABLE : 0) |
                ((!executable && nx_enabled) ? PTE_NX : 0);
    return frame;
}

void aspace_destroy(struct aspace *as) {
    if (!as) {
        return;
    }

    for (int i = 0; i < as->nframes; i++) {
        pmm_free_frame(as->frames[i]);
    }

    uint64_t tables[4] = { as->cr3, as->pdpt, as->pd, as->pt };

    for (int i = 0; i < 4; i++) {
        if (tables[i]) {
            pmm_free_frame(tables[i]);
        }
    }

    kfree(as);
}

int aspace_user_range_ok(const struct aspace *as, uint64_t addr, uint64_t len, int write) {
    if (!as || addr < USER_BASE || addr >= USER_LIMIT || len > USER_LIMIT - addr) {
        return 0;
    }

    const uint64_t *pt = (const uint64_t *)(uintptr_t)as->pt;
    uint64_t first = (addr - USER_BASE) >> 12;
    uint64_t last = (addr + (len ? len : 1) - 1 - USER_BASE) >> 12;

    for (uint64_t i = first; i <= last; i++) {
        uint64_t e = pt[i];

        if (!(e & PTE_PRESENT) || !(e & PTE_USER) || (write && !(e & PTE_WRITABLE))) {
            return 0;
        }
    }

    return 1;
}
