#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include <stdint.h>

/*
 * Thin wrappers around privileged x86_64 instructions. They are `static
 * inline` so each one is a single instruction at the call site - read the
 * asm strings to see exactly what the CPU is asked to do.
 */

#define MSR_EFER 0xC0000080u

#define RFLAGS_IF 0x200u   /* interrupt enable flag */

/* The operand of sgdt/sidt: a 16-bit limit followed by a 64-bit base. */
struct dtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static inline uint64_t read_cr0(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr4(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(v));
    return v;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b,
                         uint32_t *c, uint32_t *d) {
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf), "c"(sub));
}

static inline uint64_t read_rflags(void) {
    uint64_t v;
    __asm__ volatile ("pushfq; pop %0" : "=r"(v));
    return v;
}

static inline uint16_t read_cs(void) {
    uint16_t v;
    __asm__ volatile ("mov %%cs, %0" : "=r"(v));
    return v;
}

static inline uint16_t read_ss(void) {
    uint16_t v;
    __asm__ volatile ("mov %%ss, %0" : "=r"(v));
    return v;
}

static inline uint16_t read_ds(void) {
    uint16_t v;
    __asm__ volatile ("mov %%ds, %0" : "=r"(v));
    return v;
}

static inline void sgdt(struct dtr *out) {
    __asm__ volatile ("sgdt %0" : "=m"(*out));
}

static inline void sidt(struct dtr *out) {
    __asm__ volatile ("sidt %0" : "=m"(*out));
}

/*
 * Critical sections on a single CPU: save RFLAGS and disable interrupts,
 * later put the old interrupt state back. Nesting works because each caller
 * restores exactly what it saw.
 */
static inline uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    if (flags & RFLAGS_IF) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

#endif
