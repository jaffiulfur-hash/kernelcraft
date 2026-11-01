#include "sched/thread.h"
#include "arch/cpu.h"
#include "arch/gdt.h"
#include "arch/pit.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/aspace.h"
#include "mm/heap.h"
#include "panic.h"

#define STACK_CANARY 0xC0FFEE5AFEDEAD01ull

extern void context_switch(uint64_t *save_old_rsp, uint64_t new_rsp);

static struct thread threads[MAX_THREADS];
static struct thread *current;
static int next_id;
static int preemption;
static int slice_ticks;
static uint64_t total_switches;

const char *thread_state_name(enum thread_state s) {
    switch (s) {
        case THREAD_READY:    return "ready";
        case THREAD_RUNNING:  return "running";
        case THREAD_SLEEPING: return "sleeping";
        case THREAD_ZOMBIE:   return "zombie";
        default:              return "unused";
    }
}

/* ---- picking the next thread ----------------------------------------- */

static struct thread *pick_next(void) {
    int start = (int)(current - threads);

    /* Round robin: scan the slots after the current one, wrapping around.
     * The last slot visited is the current thread itself. */
    for (int i = 1; i <= MAX_THREADS; i++) {
        struct thread *t = &threads[(start + i) % MAX_THREADS];

        if (t->state == THREAD_READY ||
            (t == current && t->state == THREAD_RUNNING)) {
            return t;
        }
    }

    return &threads[0];   /* unreachable: main never sleeps or exits */
}

static void check_canary(const struct thread *t) {
    if (t->stack && *(const uint64_t *)t->stack != STACK_CANARY) {
        kprintf("thread '%s' (id %d) overflowed its stack\n", t->name, t->id);
        PANIC("kernel stack overflow");
    }
}

/*
 * The one place a context switch happens. Interrupts are off while we
 * pick and switch; the saved flags live on THIS thread's stack, so each
 * thread gets back exactly the interrupt state it had when it left.
 */
static void schedule(void) {
    uint64_t flags = irq_save();
    struct thread *prev = current;
    struct thread *next = pick_next();

    if (next != prev) {
        if (prev->state == THREAD_RUNNING) {
            prev->state = THREAD_READY;
        }
        next->state = THREAD_RUNNING;
        next->switches++;
        total_switches++;
        slice_ticks = 0;
        current = next;

        check_canary(prev);
        check_canary(next);

        /*
         * Switch address spaces. The kernel's own gigabyte is mapped
         * identically in every space, so the code we are running and this
         * stack stay valid across the write.
         */
        if (read_cr3() != next->cr3) {
            write_cr3(next->cr3);
        }

        /* A trap from ring 3 must land on the stack of whoever runs next. */
        if (next->is_user) {
            gdt_set_kernel_stack((uint64_t)(uintptr_t)next->stack + THREAD_STACK_SIZE);
        }

        context_switch(&prev->rsp, next->rsp);
        /* ...we are back: someone switched to `prev` again. */
    }

    irq_restore(flags);
}

/* ---- thread life cycle ------------------------------------------------ */

static void thread_trampoline(void) {
    /* We arrive here from a context switch, possibly inside an interrupt
     * handler of another thread, so interrupts may still be disabled. */
    __asm__ volatile ("sti");

    current->entry(current->arg);
    thread_exit();
}

uint64_t sched_trampoline_address(void) {
    return (uint64_t)(uintptr_t)thread_trampoline;
}

void sched_init(void) {
    memset(threads, 0, sizeof(threads));

    threads[0].id = 0;
    threads[0].state = THREAD_RUNNING;
    memcpy(threads[0].name, "main", 5);
    threads[0].cr3 = aspace_kernel_cr3();
    current = &threads[0];
    next_id = 1;
    preemption = 1;
}

static struct thread *create_common(const char *name, void (*entry)(void *), void *arg,
                                    struct aspace *as, int detached) {
    sched_reap_detached();

    uint8_t *stack = (uint8_t *)kmalloc(THREAD_STACK_SIZE);

    if (!stack) {
        return 0;
    }

    uint64_t flags = irq_save();
    struct thread *t = 0;

    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED) {
            t = &threads[i];
            break;
        }
    }

    if (!t) {
        irq_restore(flags);
        kfree(stack);
        return 0;
    }

    memset(t, 0, sizeof(*t));
    t->id = next_id++;
    t->stack = stack;
    t->entry = entry;
    t->arg = arg;
    t->as = as;
    t->is_user = as != 0;
    t->detached = (uint8_t)detached;
    t->cr3 = as ? as->cr3 : aspace_kernel_cr3();
    for (int i = 0; i < THREAD_NAME_LEN - 1 && name[i]; i++) {
        t->name[i] = name[i];
    }

    *(uint64_t *)stack = STACK_CANARY;

    /*
     * Fake the stack of a thread that is sitting inside context_switch, so
     * the very first switch to it "returns" into thread_trampoline:
     *
     *    top - 8   0                      fake return address for the trampoline
     *    top - 16  &thread_trampoline     popped by context_switch's `ret`
     *    top - 24  rbp = 0
     *    ...       rbx, r12, r13, r14
     *    top - 64  r15 = 0                <- saved rsp points here
     *
     * After the six pops and the ret, rsp = top - 8, i.e. 16-byte aligned plus
     * 8, exactly what the ABI expects on entry to a function.
     */
    uint64_t *sp = (uint64_t *)(stack + THREAD_STACK_SIZE);
    *--sp = 0;
    *--sp = (uint64_t)(uintptr_t)thread_trampoline;
    for (int i = 0; i < 6; i++) {
        *--sp = 0;
    }
    t->rsp = (uint64_t)(uintptr_t)sp;
    t->state = THREAD_READY;          /* last: now the scheduler may see it */

    irq_restore(flags);
    return t;
}

struct thread *thread_create(const char *name, void (*entry)(void *), void *arg) {
    return create_common(name, entry, arg, 0, 0);
}

struct thread *thread_create_user(const char *name, void (*entry)(void *), void *arg,
                                  struct aspace *as, int detached) {
    return create_common(name, entry, arg, as, detached);
}

struct thread *thread_current(void) {
    return current;
}

struct thread *thread_find(int id) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state != THREAD_UNUSED && threads[i].id == id) {
            return &threads[i];
        }
    }

    return 0;
}

void thread_yield(void) {
    schedule();
}

static int other_thread_ready(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (&threads[i] != current && threads[i].state == THREAD_READY) {
            return 1;
        }
    }

    return 0;
}

void thread_idle(void) {
    __asm__ volatile ("cli");

    if (other_thread_ready()) {
        schedule();
        __asm__ volatile ("sti");
    } else {
        /* Nobody else wants the CPU: halt until the next interrupt. */
        __asm__ volatile ("sti; hlt");
    }
}

void thread_sleep_ticks(uint64_t ticks) {
    uint64_t until = pit_get_ticks() + ticks;

    if (current == &threads[0]) {
        /* main must stay runnable (it is the fallback), so it just waits. */
        while (pit_get_ticks() < until) {
            thread_idle();
        }
        return;
    }

    uint64_t flags = irq_save();

    current->wake_tick = until;
    current->state = THREAD_SLEEPING;
    schedule();                       /* sched_tick() makes us READY again */
    irq_restore(flags);
}

void thread_exit(void) {
    irq_save();
    current->state = THREAD_ZOMBIE;
    schedule();

    for (;;) {                        /* a zombie is never scheduled again */
        __asm__ volatile ("hlt");
    }
}

/* Release everything a finished thread still owns. It must not be running. */
static void free_thread(struct thread *t) {
    uint64_t flags = irq_save();
    uint8_t *stack = t->stack;
    struct aspace *as = t->as;

    t->state = THREAD_UNUSED;
    t->stack = 0;
    t->as = 0;
    irq_restore(flags);

    kfree(stack);
    aspace_destroy(as);          /* no-op for kernel threads */
}

int thread_join(struct thread *t) {
    if (!t || t == current || t == &threads[0] || t->state == THREAD_UNUSED) {
        return -1;
    }

    int id = t->id;

    while (t->id == id && t->state != THREAD_ZOMBIE) {
        thread_idle();
    }

    free_thread(t);
    return 0;
}

void sched_reap_detached(void) {
    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_ZOMBIE && threads[i].detached) {
            free_thread(&threads[i]);
        }
    }
}

/* ---- timer-driven part ------------------------------------------------ */

void sched_set_preemption(int enabled) {
    preemption = enabled;
}

int sched_preemption_enabled(void) {
    return preemption;
}

void sched_tick(void) {
    if (!current) {
        return;
    }

    uint64_t now = pit_get_ticks();

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_SLEEPING && threads[i].wake_tick <= now) {
            threads[i].state = THREAD_READY;
        }
    }

    current->cpu_ticks++;

    if (preemption && ++slice_ticks >= THREAD_QUANTUM_TICKS) {
        slice_ticks = 0;
        schedule();
    }
}

/* ---- introspection ---------------------------------------------------- */

const struct thread *sched_thread_at(int slot) {
    if (slot < 0 || slot >= MAX_THREADS || threads[slot].state == THREAD_UNUSED) {
        return 0;
    }

    return &threads[slot];
}

uint64_t sched_switch_count(void) {
    return total_switches;
}
