#ifndef SCHED_THREAD_H
#define SCHED_THREAD_H

#include <stddef.h>
#include <stdint.h>

/*
 * Kernel threads and a preemptive round-robin scheduler.
 *
 * Every thread has its own stack and a saved stack pointer. "Running" a
 * thread means loading its stack pointer and returning into whatever it was
 * doing; context_switch() (switch.asm) is the only code that does that.
 * The boot context becomes thread 0, "main": it runs the shell and is always
 * runnable, which is also why there is no separate idle thread.
 */

#define MAX_THREADS        16
#define THREAD_STACK_SIZE  16384
#define THREAD_NAME_LEN    16
#define THREAD_QUANTUM_TICKS 5        /* time slice: 5 ticks = 50 ms at 100 Hz */

struct aspace;

/* How a user process ended (kept in the thread until it is joined). */
enum term_reason {
    TERM_NONE = 0,
    TERM_EXITED,          /* called exit */
    TERM_FAULT,           /* killed by the kernel after a CPU exception */
    TERM_KILLED           /* killed on request (the `kill` command) */
};

enum thread_state {
    THREAD_UNUSED = 0,
    THREAD_READY,        /* runnable, waiting for the CPU */
    THREAD_RUNNING,      /* has the CPU right now (exactly one thread) */
    THREAD_SLEEPING,     /* waiting for wake_tick */
    THREAD_ZOMBIE        /* finished; waiting to be joined and freed */
};

struct thread {
    uint64_t rsp;                    /* saved stack pointer while not running */
    int id;                          /* unique, never reused */
    enum thread_state state;
    char name[THREAD_NAME_LEN];
    uint8_t *stack;                  /* kmalloc'ed block, NULL for main */
    void (*entry)(void *);
    void *arg;
    uint64_t wake_tick;
    uint64_t cpu_ticks;              /* timer ticks spent running */
    uint64_t switches;               /* times it was given the CPU */

    /* --- user processes (all zero for plain kernel threads) --- */
    uint64_t cr3;                    /* page tables to load when this thread runs */
    struct aspace *as;               /* its private address space, if a process */
    uint8_t is_user;                 /* runs (mostly) in ring 3 */
    uint8_t detached;                /* reaped automatically when it ends */
    volatile uint8_t kill_pending;   /* die at the next return to user mode */
    enum term_reason term;
    int exit_code;
    int fault_vector;
    uint64_t fault_addr;
    uint64_t fault_rip;
};

void sched_init(void);

/* Start a thread; it is READY immediately. NULL if out of slots or memory. */
struct thread *thread_create(const char *name, void (*entry)(void *), void *arg);

/* Like thread_create, but the thread owns `as` and runs with its page tables. */
struct thread *thread_create_user(const char *name, void (*entry)(void *), void *arg,
                                  struct aspace *as, int detached);

struct thread *thread_current(void);
struct thread *thread_find(int id);

/* Free finished detached threads (stack and address space). */
void sched_reap_detached(void);

/* Give up the CPU voluntarily. Returns when this thread is scheduled again. */
void thread_yield(void);

/* Sleep for at least `ticks` timer ticks. */
void thread_sleep_ticks(uint64_t ticks);

/* Wait for something without wasting CPU: let other threads run, or halt. */
void thread_idle(void);

/* Terminate the calling thread (also happens when its entry function returns). */
void thread_exit(void) __attribute__((noreturn));

/* Wait for `t` to finish, then free its stack. 0 on success, -1 if invalid. */
int thread_join(struct thread *t);

/* Turn timer-driven preemption on or off (voluntary yields still work). */
void sched_set_preemption(int enabled);
int sched_preemption_enabled(void);

/* Called by the timer interrupt (after EOI): wakes sleepers, may preempt. */
void sched_tick(void);

/* Introspection for `ps`, the lessons and the tests. */
const struct thread *sched_thread_at(int slot);
uint64_t sched_switch_count(void);
uint64_t sched_trampoline_address(void);
const char *thread_state_name(enum thread_state s);

#endif
