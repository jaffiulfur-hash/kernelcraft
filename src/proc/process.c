#include "proc/process.h"
#include "arch/cpu.h"
#include "arch/gdt.h"
#include "arch/interrupts.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/aspace.h"
#include "mm/heap.h"
#include "proc/elf.h"

extern void enter_user_mode(uint64_t entry, uint64_t user_rsp);

struct start_info {
    uint64_t entry;
    uint64_t rsp;
};

const struct user_program *process_find_program(const char *name) {
    for (uint64_t i = 0; i < user_program_count; i++) {
        if (strcmp(user_programs[i].name, name) == 0) {
            return &user_programs[i];
        }
    }

    return 0;
}

/* First code to run in a new process's thread: still ring 0, then it "returns" to ring 3. */
static void user_entry(void *arg) {
    struct start_info info = *(struct start_info *)arg;

    kfree(arg);

    /* From now on, a trap from ring 3 must land on THIS thread's kernel stack. */
    gdt_set_kernel_stack((uint64_t)(uintptr_t)thread_current()->stack + THREAD_STACK_SIZE);
    enter_user_mode(info.entry, info.rsp);
}

struct thread *process_spawn(const char *name, int detached, const char **why) {
    const char *err = 0;
    const struct user_program *prog = process_find_program(name);
    struct aspace *as = 0;
    struct start_info *info = 0;
    uint64_t entry = 0;
    struct thread *t = 0;

    if (!prog) {
        err = "no such program";
        goto fail;
    }

    as = aspace_create();
    if (!as) {
        err = "out of memory";
        goto fail;
    }

    err = elf_load(as, prog->start, (size_t)(prog->end - prog->start), &entry);
    if (err) {
        goto fail;
    }

    for (int i = 1; i <= USER_STACK_PAGES; i++) {
        if (!aspace_map_new_page(as, USER_STACK_TOP - (uint64_t)i * 4096, 1, 0)) {
            err = "out of memory for the stack";
            goto fail;
        }
    }

    info = (struct start_info *)kmalloc(sizeof(*info));
    if (!info) {
        err = "out of memory";
        goto fail;
    }
    info->entry = entry;
    info->rsp = USER_STACK_TOP;

    t = thread_create_user(prog->name, user_entry, info, as, detached);
    if (!t) {
        err = "no free thread slot";
        goto fail;
    }

    return t;

fail:
    if (info) kfree(info);
    if (as) aspace_destroy(as);
    if (why) *why = err;
    return 0;
}

int process_wait(struct thread *t, struct process_result *out) {
    if (!t) {
        return -1;
    }

    while (t->state != THREAD_ZOMBIE) {
        thread_idle();
    }

    if (out) {
        out->pid = t->id;
        memcpy(out->name, t->name, THREAD_NAME_LEN);
        out->term = t->term;
        out->exit_code = t->exit_code;
        out->fault_vector = t->fault_vector;
        out->fault_addr = t->fault_addr;
        out->fault_rip = t->fault_rip;
    }

    return thread_join(t);       /* frees the kernel stack and the address space */
}

int process_kill(int pid) {
    struct thread *t = thread_find(pid);

    if (!t || !t->is_user || t->state == THREAD_ZOMBIE) {
        return -1;
    }

    t->kill_pending = 1;
    return 0;
}

void process_exit_current(int code) {
    struct thread *t = thread_current();

    t->term = TERM_EXITED;
    t->exit_code = code;
    thread_exit();
}

void process_fault(const interrupt_frame_t *frame) {
    struct thread *t = thread_current();

    t->term = TERM_FAULT;
    t->fault_vector = (int)frame->vector;
    t->fault_rip = frame->rip;
    t->fault_addr = frame->vector == 14 ? read_cr2() : 0;
    t->exit_code = -(int)frame->vector;

    kprintf("[kernel] process %d (%s) killed: %s\n", t->id, t->name,
            interrupt_vector_name((uint8_t)frame->vector));
    kprintf("         rip=0x%lx", frame->rip);
    if (frame->vector == 14) {
        kprintf("  address=0x%lx  (%s, %s)", t->fault_addr,
                (frame->error_code & 1) ? "protection violation" : "page not present",
                (frame->error_code & 2) ? "write" : "read");
    }
    kprintf("\n");

    thread_exit();
}

void process_check_kill(void) {
    struct thread *t = thread_current();

    if (t->kill_pending) {
        t->term = TERM_KILLED;
        t->exit_code = -9;
        kprintf("[kernel] process %d (%s) killed on request\n", t->id, t->name);
        thread_exit();
    }
}
