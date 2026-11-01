#include "syscall/syscall.h"
#include "arch/idt.h"
#include "arch/isr.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "layout.h"
#include "mm/aspace.h"
#include "proc/process.h"
#include "sched/thread.h"

#define WRITE_MAX 256

static const char *const names[SYSCALL_COUNT] = {
    "write", "getpid", "yield", "uptime_ms", "sleep_ms", "exit"
};

const char *syscall_name(uint64_t number) {
    return number < SYSCALL_COUNT ? names[number] : "(unknown)";
}

void syscall_init(void) {
    /*
     * 0xEE = present, DPL 3, interrupt gate. The DPL is the point: the CPU
     * only lets code in a ring numerically <= DPL execute `int 0x80`, so
     * ring 3 may use THIS gate - and, since every other gate is DPL 0, no
     * other one.
     */
    idt_set_gate(SYSCALL_VECTOR, isr128, 0xEE);
}

/*
 * Never trust a pointer that comes from the caller. Even though today's
 * callers are kernel code, this is the check a real kernel makes before it
 * touches user memory: the whole range must lie where it is allowed to.
 */
static int range_ok(uint64_t addr, uint64_t len, int from_user) {
    if (from_user) {
        /* Ring 3 may only hand us pointers into its OWN mapped pages. */
        return len <= WRITE_MAX &&
               aspace_user_range_ok(thread_current()->as, addr, len, 0);
    }

    return len <= WRITE_MAX && addr < BOOT_IDENTITY_MAP_BYTES &&
           len <= BOOT_IDENTITY_MAP_BYTES - addr;
}

void syscall_dispatch(interrupt_frame_t *frame) {
    uint64_t result = (uint64_t)-1;
    int from_user = (frame->cs & 3) == 3;

    switch (frame->rax) {
        case SYS_WRITE:
            if (range_ok(frame->rdi, frame->rsi, from_user)) {
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
            if (from_user) {
                process_exit_current((int)frame->rdi);
            }
            thread_exit();
        default:
            break;
    }

    /* The register image is popped back by the ISR stub: this IS the return value. */
    frame->rax = result;
}
