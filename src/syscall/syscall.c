#include "syscall/syscall.h"
#include "arch/idt.h"
#include "arch/isr.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "layout.h"
#include "sched/thread.h"

#define WRITE_MAX 256

static const char *const names[SYSCALL_COUNT] = {
    "write", "getpid", "yield", "uptime_ms", "sleep_ms", "exit"
};

const char *syscall_name(uint64_t number) {
    return number < SYSCALL_COUNT ? names[number] : "(unknown)";
}

void syscall_init(void) {
    /* 0x8E = present, DPL 0, interrupt gate. A ring 3 caller would need 0xEE. */
    idt_set_gate(SYSCALL_VECTOR, isr128, 0x8E);
}

/*
 * Never trust a pointer that comes from the caller. Even though today's
 * callers are kernel code, this is the check a real kernel makes before it
 * touches user memory: the whole range must lie where it is allowed to.
 */
static int range_ok(uint64_t addr, uint64_t len) {
    return len <= WRITE_MAX && addr < BOOT_IDENTITY_MAP_BYTES &&
           len <= BOOT_IDENTITY_MAP_BYTES - addr;
}

void syscall_dispatch(interrupt_frame_t *frame) {
    uint64_t result = (uint64_t)-1;

    switch (frame->rax) {
        case SYS_WRITE:
            if (range_ok(frame->rdi, frame->rsi)) {
                const char *buf = (const char *)(uintptr_t)frame->rdi;

                for (uint64_t i = 0; i < frame->rsi; i++) {
                    log_write_char(buf[i]);
                }
                result = frame->rsi;
            }
            break;
        case SYS_GETPID:
            result = (uint64_t)thread_current()->id;
            break;
        case SYS_YIELD:
            thread_yield();
            result = 0;
            break;
        case SYS_UPTIME_MS:
            result = pit_get_ticks() * 1000 / PIT_HZ;
            break;
        case SYS_SLEEP_MS:
            thread_sleep_ticks((frame->rdi * PIT_HZ + 999) / 1000);
            result = 0;
            break;
        case SYS_EXIT:
            thread_exit();
        default:
            break;
    }

    /* The register image is popped back by the ISR stub: this IS the return value. */
    frame->rax = result;
}
