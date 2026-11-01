#ifndef ULIB_H
#define ULIB_H

#include <stddef.h>
#include <stdint.h>
#include "syscall/numbers.h"

/* The user-side face of the system call ABI: rax = number, rdi/rsi/rdx args. */
static inline long usyscall(long n, long a, long b, long c) {
    long result;

    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(n), "D"(a), "S"(b), "d"(c)
                      : "memory");
    return result;
}

static inline long ugetpid(void) { return usyscall(SYS_GETPID, 0, 0, 0); }
static inline void uyield(void) { usyscall(SYS_YIELD, 0, 0, 0); }
static inline void usleep_ms(unsigned long ms) { usyscall(SYS_SLEEP_MS, (long)ms, 0, 0); }
static inline unsigned long uuptime_ms(void) { return (unsigned long)usyscall(SYS_UPTIME_MS, 0, 0, 0); }

/* Which ring am I in? The low two bits of CS (3 for user mode). */
static inline unsigned ucs(void) {
    unsigned short cs;

    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}

void uputs(const char *s);
void uputdec(unsigned long v);
void uputhex(unsigned long v);

#endif
