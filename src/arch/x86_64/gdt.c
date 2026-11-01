#include "arch/gdt.h"
#include "lib/string.h"
#include <stdint.h>

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint64_t base;
};

/* The 64-bit Task State Segment. Hardware task switching is gone in long
 * mode; what remains is the stack table used on privilege changes. */
struct __attribute__((packed)) tss {
    uint32_t reserved0;
    uint64_t rsp0;           /* stack loaded on a ring 3 -> ring 0 transition */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];         /* optional dedicated interrupt stacks */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;     /* >= sizeof(tss): no I/O permission bitmap */
};

/* null, kernel code, kernel data, user data, user code, TSS (two slots). */
static struct gdt_entry gdt[7];
static struct gdt_ptr gp;
static struct tss tss;

extern void gdt_flush(uint64_t gdt_ptr_addr);

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[idx].limit_low = (uint16_t)(limit & 0xFFFF);
    gdt[idx].base_low = (uint16_t)(base & 0xFFFF);
    gdt[idx].base_mid = (uint8_t)((base >> 16) & 0xFF);
    gdt[idx].access = access;
    gdt[idx].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    gdt[idx].base_high = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void) {
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0, 0x9A, 0x20);   /* P, ring 0, code, readable, L=1 */
    gdt_set_entry(2, 0, 0, 0x92, 0x00);   /* P, ring 0, data, writable */
    gdt_set_entry(3, 0, 0, 0xF2, 0x00);   /* P, ring 3, data, writable */
    gdt_set_entry(4, 0, 0, 0xFA, 0x20);   /* P, ring 3, code, readable, L=1 */

    /* A system descriptor for the TSS is 16 bytes: it takes two GDT slots. */
    memset(&tss, 0, sizeof(tss));
    tss.iomap_base = sizeof(tss);

    uint64_t base = (uint64_t)(uintptr_t)&tss;

    gdt_set_entry(5, (uint32_t)base, sizeof(tss) - 1, 0x89, 0x00);  /* P, type 9 = 64-bit TSS */
    *(uint64_t *)&gdt[6] = base >> 32;                              /* upper half of the base */

    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base = (uint64_t)(uintptr_t)&gdt[0];

    gdt_flush((uint64_t)(uintptr_t)&gp);

    /* ltr: tell the CPU where the TSS descriptor is. */
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)GDT_TSS));
}

void gdt_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

uint64_t gdt_get_kernel_stack(void) {
    return tss.rsp0;
}

uint64_t gdt_tss_address(void) {
    return (uint64_t)(uintptr_t)&tss;
}
