#include "arch/gdt.h"
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

static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

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
    gp.limit = (uint16_t)(sizeof(gdt) - 1);
    gp.base = (uint64_t)&gdt[0];

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0, 0x9A, 0x20);
    gdt_set_entry(2, 0, 0, 0x92, 0x00);

    gdt_flush((uint64_t)&gp);
}
