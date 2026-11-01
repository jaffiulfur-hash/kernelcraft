#ifndef SCHED_SYNC_H
#define SCHED_SYNC_H

#include "sched/thread.h"

/*
 * A mutex for kernel threads on one CPU.
 *
 * The test-and-set is a single atomic instruction (xchg), so two threads can
 * never both see "unlocked". A thread that finds the lock taken yields
 * instead of spinning: on one CPU the holder cannot run until we give up the
 * CPU, so spinning would only burn our time slice.
 */
struct mutex {
    volatile int locked;
};

#define MUTEX_INIT { 0 }

static inline void mutex_lock(struct mutex *m) {
    while (__atomic_exchange_n(&m->locked, 1, __ATOMIC_ACQUIRE)) {
        thread_yield();
    }
}

static inline void mutex_unlock(struct mutex *m) {
    __atomic_store_n(&m->locked, 0, __ATOMIC_RELEASE);
}

#endif
