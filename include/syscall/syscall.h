#ifndef SYSCALL_SYSCALL_H
#define SYSCALL_SYSCALL_H

#include <stdint.h>
#include "arch/isr.h"

/*
 * System calls: the controlled doorway into the kernel.
 *
 * ABI (as on 32-bit Linux's int 0x80, widened to 64 bits):
 *     rax = call number, rdi/rsi/rdx = arguments, result comes back in rax.
 * Unknown numbers and bad arguments return -1.
 *
 * NOTE: KernelCraft has no user mode yet, so callers are kernel code in
 * ring 0 and the gate has DPL 0. This is the dispatch half of a system
 * call; the privilege-change half (GDT user segments, TSS, US page bits,
 * a DPL 3 gate) is the next stage.
 */

#define SYSCALL_VECTOR 0x80

#define SYS_WRITE      0   /* (const char *buf, size_t len)  -> bytes written */
#define SYS_GETPID     1   /* ()                              -> thread id */
#define SYS_YIELD      2   /* ()                              -> 0 */
#define SYS_UPTIME_MS  3   /* ()                              -> milliseconds since boot */
#define SYS_SLEEP_MS   4   /* (uint64_t ms)                   -> 0 */
#define SYS_EXIT       5   /* ()                              -> does not return */
#define SYSCALL_COUNT  6

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
