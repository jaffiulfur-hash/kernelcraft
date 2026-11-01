#ifndef PROC_PROCESS_H
#define PROC_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include "arch/isr.h"
#include "sched/thread.h"

/*
 * User processes. A process is a kernel thread with two extra things: its own
 * address space (see mm/aspace.h) and a ring 3 side. The thread's kernel stack
 * is what the CPU switches to whenever the program traps into the kernel.
 */

/* One embedded program, as produced by tools/mkblob.sh. */
struct user_program {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
};

extern const struct user_program user_programs[];
extern const uint64_t user_program_count;

const struct user_program *process_find_program(const char *name);

/*
 * Load `name` into a fresh address space and start it. NULL on failure
 * (*why says why when non-NULL). A detached process is reaped automatically
 * when it ends; otherwise somebody must process_wait() for it.
 */
struct thread *process_spawn(const char *name, int detached, const char **why);

struct process_result {
    int pid;
    char name[THREAD_NAME_LEN];
    enum term_reason term;
    int exit_code;
    int fault_vector;
    uint64_t fault_addr;
    uint64_t fault_rip;
};

/* Wait for the process to end, collect how it ended, and free it. */
int process_wait(struct thread *t, struct process_result *out);

/* Ask a process to die at its next return to user mode. 0, or -1 if no such process. */
int process_kill(int pid);

/* Called by the interrupt code. */
void process_fault(const interrupt_frame_t *frame) __attribute__((noreturn));
void process_exit_current(int code) __attribute__((noreturn));
void process_check_kill(void);

#endif
