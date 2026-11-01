#include "learn/learn.h"
#include "arch/pit.h"
#include "lib/kprintf.h"
#include "sched/thread.h"

#define WORKERS 3

static volatile int stop_flag;
static volatile uint64_t work[WORKERS];

static void nothing(void *arg) {
    (void)arg;
}

static void stepper(void *arg) {
    const char *name = (const char *)arg;

    for (int step = 1; step <= 4; step++) {
        kprintf_atomic("%s%d ", name, step);
        thread_yield();
    }
}

static void busy(void *arg) {
    int i = (int)(intptr_t)arg;

    while (!stop_flag) {
        work[i]++;                 /* never yields: only the timer can stop it */
    }
}

static void napper(void *arg) {
    (void)arg;
    while (!stop_flag) {
        thread_sleep_ticks(20);
    }
}

static void print_threads(void) {
    kprintf("  id name        state     cpu ticks  switches\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        const struct thread *t = sched_thread_at(i);

        if (t) {
            kprintf("  %2d %-11s %-9s %9lu  %8lu\n", t->id, t->name,
                    thread_state_name(t->state), t->cpu_ticks, t->switches);
        }
    }
}

void lesson_sched(void) {
    learn_page("Threads and the scheduler");
    learn_heading("Many things at once, on one CPU");
    learn_para("A CPU runs one instruction stream at a time. Multitasking is a "
               "trick: run thread A for a few milliseconds, save exactly where "
               "it was, run B, and so on, fast enough that it looks "
               "simultaneous. Each thread owns a stack and a saved stack "
               "pointer; together with a state and bookkeeping that is the "
               "thread control block (struct thread in sched/thread.h).");
    kprintf("   thread_create        pick_next()\n");
    kprintf("  ---------------> READY ----------> RUNNING --- thread_exit ---> ZOMBIE\n");
    kprintf("                     ^  <---------- /  |    \\                      |\n");
    kprintf("          timer wake |  yield or end  |     \\ thread_sleep     join frees\n");
    kprintf("                     |  of time slice |      v                  the stack\n");
    kprintf("                     +------------ SLEEPING <+\n\n");
    learn_para("The policy here is round robin: the next READY thread in the "
               "table, wrapping around. The PIT interrupt makes it preemptive - "
               "every tick sched_tick() counts the running thread's time, and "
               "after a quantum of 5 ticks (50 ms) it forces a switch even if "
               "the thread never asked.");
    LEARN_NEXT();

    learn_page("Threads and the scheduler");
    learn_heading("The context switch, in nine instructions");
    kprintf("  context_switch(&old->rsp, new->rsp):\n");
    kprintf("      push rbp, rbx, r12, r13, r14, r15    ; save A's registers on A's stack\n");
    kprintf("      mov [rdi], rsp                       ; remember where A's stack ended\n");
    kprintf("      mov rsp, rsi                         ; --- now running on B's stack ---\n");
    kprintf("      pop r15, r14, r13, r12, rbx, rbp     ; restore B's registers\n");
    kprintf("      ret                                  ; return into B, where B left off\n\n");
    learn_para("Only six registers are saved because the C calling convention "
               "lets any function clobber the others, so the compiler already "
               "saved what it needs around the call. Nothing runs between "
               "the two stacks: it is the stack itself that carries the "
               "thread's whole execution state.");
    LEARN_NEXT();

    learn_page("Threads and the scheduler");
    sched_set_preemption(0);
    struct thread *t = thread_create("demo", nothing, 0);

    learn_live("a brand-new thread that has never run");
    if (t) {
        const uint64_t *sp = (const uint64_t *)(uintptr_t)t->rsp;
        static const char *const names[8] = { "r15", "r14", "r13", "r12", "rbx", "rbp",
                                              "return address", "(fake caller)" };

        kprintf("  stack block 0x%lx, saved rsp = 0x%lx\n", (uint64_t)(uintptr_t)t->stack,
                t->rsp);
        for (int i = 0; i < 8; i++) {
            kprintf("    rsp+%-2d  0x%016lx  %s%s\n", i * 8, sp[i], names[i],
                    (i == 6 && sp[i] == sched_trampoline_address()) ?
                        "  = thread_trampoline" : "");
        }
        thread_join(t);
    }
    sched_set_preemption(1);
    kprintf("\n");
    learn_para("create_thread() builds that stack by hand so the FIRST switch "
               "'returns' into thread_trampoline, which enables interrupts and "
               "calls the entry function. A thread that has run before looks "
               "the same, except the return address is wherever it called "
               "context_switch from.");
    LEARN_NEXT();

    learn_page("Threads and the scheduler");
    learn_live("cooperative: three threads that politely yield (preemption off)");
    sched_set_preemption(0);

    struct thread *c[3];
    static const char *const labels[3] = { "A", "B", "C" };

    for (int i = 0; i < 3; i++) {
        c[i] = thread_create(labels[i], stepper, (void *)labels[i]);
    }
    kprintf("  ");
    for (int i = 0; i < 3; i++) {
        if (c[i]) {
            thread_join(c[i]);
        }
    }
    kprintf("\n\n");
    sched_set_preemption(1);
    learn_para("A1 B1 C1 A2 B2 C2 ...: perfect alternation, because each thread "
               "hands the CPU to the next READY one in slot order. That is "
               "round robin. It only works while every thread cooperates - one "
               "that loops forever without yielding would freeze the machine. "
               "Early systems (Windows 3.1, classic Mac OS) did exactly this.");
    LEARN_NEXT();

    learn_page("Threads and the scheduler");
    learn_live("preemptive: three threads that never yield, one that sleeps");
    stop_flag = 0;
    for (int i = 0; i < WORKERS; i++) {
        work[i] = 0;
    }

    struct thread *w[WORKERS];
    static const char *const wnames[WORKERS] = { "worker-1", "worker-2", "worker-3" };

    for (int i = 0; i < WORKERS; i++) {
        w[i] = thread_create(wnames[i], busy, (void *)(intptr_t)i);
    }
    struct thread *nap = thread_create("napper", napper, 0);

    thread_sleep_ticks(60);
    kprintf("  snapshot after 0.6 s (like the 'ps' command):\n");
    print_threads();

    thread_sleep_ticks(40);
    stop_flag = 1;
    while ((w[0] && w[0]->state != THREAD_ZOMBIE) || (w[1] && w[1]->state != THREAD_ZOMBIE) ||
           (w[2] && w[2]->state != THREAD_ZOMBIE) || (nap && nap->state != THREAD_ZOMBIE)) {
        thread_idle();
    }

    kprintf("\n  after 1 s:  thread     loop iterations   cpu ticks   switches\n");
    for (int i = 0; i < WORKERS; i++) {
        if (w[i]) {
            kprintf("              %-10s %15lu %11lu %10lu\n", w[i]->name, work[i],
                    w[i]->cpu_ticks, w[i]->switches);
        }
    }
    for (int i = 0; i < WORKERS; i++) {
        if (w[i]) {
            thread_join(w[i]);
        }
    }
    if (nap) {
        thread_join(nap);
    }
    kprintf("\n");
    learn_para("The three workers never call yield, yet they take equal turns: "
               "the timer interrupt preempts them every 5 ticks. The sleeper "
               "shows SLEEPING and uses almost no CPU, which is what a blocked "
               "thread should cost. main is shown too - it is just another "
               "thread (the shell). Each worker earned roughly a third of the "
               "ticks: fair share.");
    LEARN_NEXT();

    learn_page("Threads and the scheduler");
    learn_heading("What this scheduler leaves out");
    kprintf("  priorities        every thread is equal; real systems favour the\n");
    kprintf("                    interactive over the number-cruncher\n");
    kprintf("  blocking on I/O   a thread waiting for a key or a disk should sleep on a\n");
    kprintf("                    wait queue, not poll; the device's IRQ wakes it\n");
    kprintf("  idle thread       we halt inside 'main' instead of running a dummy\n");
    kprintf("  many CPUs         per-CPU run queues, load balancing, IPIs\n");
    kprintf("  processes         each with its own page tables (CR3) and a ring 3 side\n");
    kprintf("  fairness maths    Linux CFS tracks virtual runtime in a red-black tree\n\n");
    learn_para("What you saw is the real core: a thread table, a saved stack "
               "pointer per thread, a switch routine, and a timer that "
               "enforces turns. Everything fancier is policy built on top.");
    learn_try("ps    (run it while a lesson is waiting? no - try 'learn sync' next)");
    learn_try("set THREAD_QUANTUM_TICKS to 1 in thread.h, rebuild, and compare "
              "the switch counts here.");
}
