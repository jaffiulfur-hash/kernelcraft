#ifndef SYSCALL_SYSCALL_H
#define SYSCALL_SYSCALL_H

#include <stdint.h>
#include "arch/isr.h"
#include "syscall/numbers.h"

/*
 * System calls: the controlled doorway into the kernel. Numbers are in
 * syscall/numbers.h so user programs can share them.
 *
 * ABI (as on 32-bit Linux's int 0x80, widened to 64 bits):
 *     rax = call number, rdi/rsi/rdx = arguments, result comes back in rax.
 * Unknown numbers and bad arguments return -1.
 *
 * Callers may be kernel threads (ring 0) or user processes (ring 3). The gate
 * has DPL 3, so `int 0x80` is legal from user mode; every other vector is
 * closed to it. Pointers from ring 3 are checked against the caller's own
 * address space before the kernel touches them.
 */

void syscall_init(void);                       /* installs the IDT gate */
void syscall_dispatch(interrupt_frame_t *frame);
const char *syscall_name(uint64_t number);

/* Invoke a system call the way a program would: with `int 0x80`. */
static inline long syscall3(long number, long a, long b, long c) {
    long result;

    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "D"(a), "S"(b), "d"(c)
                      : "memory");
    return result;
}

#endif
