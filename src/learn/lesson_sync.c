#include "learn/learn.h"
#include "lib/kprintf.h"
#include "sched/sync.h"
#include "sched/thread.h"

static volatile uint64_t shared;
static struct mutex lock = MUTEX_INIT;

static void worker(void *arg) {
    int mode = (int)(intptr_t)arg;

    for (int i = 0; i < SYNC_ITERS; i++) {
        switch (mode) {
            case SYNC_UNSAFE: {
                uint64_t v = shared;     /* read the counter ... */
                thread_yield();          /* ... another thread runs now ... */
                shared = v + 1;          /* ... write back a stale value */
                break;
            }
            case SYNC_MUTEX:
                mutex_lock(&lock);
                {
                    uint64_t v = shared;
                    thread_yield();      /* harmless: nobody else may enter */
                    shared = v + 1;
                }
                mutex_unlock(&lock);
                break;
            default:
                __atomic_fetch_add(&shared, 1, __ATOMIC_SEQ_CST);
                thread_yield();
                break;
        }
    }
}

uint64_t sync_demo_race(int mode) {
    struct thread *t[SYNC_THREADS];
    int was = sched_preemption_enabled();

    shared = 0;
    sched_set_preemption(0);     /* yields alone decide when threads switch */
    for (int i = 0; i < SYNC_THREADS; i++) {
        t[i] = thread_create("racer", worker, (void *)(intptr_t)mode);
    }
    for (int i = 0; i < SYNC_THREADS; i++) {
        if (t[i]) {
            thread_join(t[i]);
        }
    }
    sched_set_preemption(was);

    return shared;
}

void lesson_sync(void) {
    learn_page("Races and locks");
    learn_heading("The bug that only happens sometimes");
    learn_para("Two threads add 1 to the same counter. 'counter++' looks like "
               "one step but the CPU does three: load it into a register, add, "
               "store it back. If a thread is switched out between the load "
               "and the store, the other one's work is overwritten:");
    kprintf("    thread A                 thread B                 counter\n");
    kprintf("    v = counter   (v = 0)                                0\n");
    kprintf("                             v = counter   (v = 0)       0\n");
    kprintf("    counter = v + 1                                      1\n");
    kprintf("                             counter = v + 1             1   <- LOST\n\n");
    learn_para("Two increments, result 1. This is a race condition, and the "
               "stretch of code that must not be interleaved is a critical "
               "section. With a timer that preempts at arbitrary moments the "
               "failure is rare and unrepeatable - the worst kind of bug. "
               "Here a thread_yield() in the middle of the window forces the "
               "unlucky timing every time, so you can study it.");
    LEARN_NEXT();

    learn_page("Races and locks");
    learn_live("3 threads x 50 increments each; the right answer is 150");
    uint64_t unsafe = sync_demo_race(SYNC_UNSAFE);

    kprintf("  no protection:           counter = %lu   (lost %lu updates)\n\n", unsafe,
            (uint64_t)(SYNC_ITERS * SYNC_THREADS) - unsafe);
    learn_para("Every thread read the counter, yielded, and wrote back "
               "its stale value + 1, so the three of them together advance "
               "the count by only one per round. The CPU did nothing wrong - "
               "the program told it to do exactly this.");
    LEARN_NEXT();

    learn_page("Races and locks");
    learn_live("two fixes, same threads, same yields");
    uint64_t locked = sync_demo_race(SYNC_MUTEX);
    uint64_t atomic = sync_demo_race(SYNC_ATOMIC);

    kprintf("  strategy                       result   expected\n");
    kprintf("  no protection                  %6lu   %6d\n", unsafe, SYNC_ITERS * SYNC_THREADS);
    kprintf("  mutex around read/yield/write  %6lu   %6d\n", locked, SYNC_ITERS * SYNC_THREADS);
    kprintf("  one atomic add (lock xadd)     %6lu   %6d\n\n", atomic, SYNC_ITERS * SYNC_THREADS);
    learn_para("The mutex lets one thread at a time into the critical section. "
               "Taking it needs an indivisible test-and-set - sched/sync.h uses "
               "xchg, which the CPU performs as a single uninterruptible step. "
               "The atomic add shrinks the critical section to one instruction "
               "so there is no window left to protect.");
    LEARN_NEXT();

    learn_page("Races and locks");
    learn_heading("Choosing a tool");
    kprintf("  interrupts off     what kmalloc does (irq_save): on ONE cpu nothing else\n");
    kprintf("                     can run, but it stalls the timer and the keyboard, so\n");
    kprintf("                     keep it very short\n");
    kprintf("  mutex              holder keeps the lock for a while; waiters sleep or\n");
    kprintf("                     yield instead of burning CPU\n");
    kprintf("  spinlock           waiters spin; only sensible on several CPUs and for\n");
    kprintf("                     tiny sections (spinning on one CPU waits for a thread\n");
    kprintf("                     that cannot run until you give up the CPU)\n");
    kprintf("  atomic operations  lock xadd, cmpxchg: lock-free counters and queues\n\n");
    learn_para("Locks bring their own bugs. Deadlock: thread 1 holds lock A and "
               "waits for B while thread 2 holds B and waits for A - neither "
               "can ever proceed. The standard cure is a global lock ordering. "
               "Priority inversion lets a low-priority thread holding a lock "
               "block a high-priority one; the Mars Pathfinder famously "
               "rebooted from it.");
    learn_try("exercise: write two threads that deadlock on two mutexes, then "
              "use 'ps' to watch them spin; fix it by ordering the locks.");
}
