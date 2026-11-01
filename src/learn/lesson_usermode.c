#include "learn/learn.h"
#include "arch/cpu.h"
#include "arch/gdt.h"
#include "arch/interrupts.h"
#include "lib/kprintf.h"
#include "proc/process.h"
#include "syscall/numbers.h"

/* Run a program to completion; the program prints its own output. */
static void run_and_report(const char *name) {
    const char *why = 0;
    struct thread *t = process_spawn(name, 0, &why);
    struct process_result r;

    if (!t) {
        kprintf("  cannot start %s: %s\n", name, why ? why : "?");
        return;
    }

    process_wait(t, &r);
    if (r.term == TERM_EXITED) {
        kprintf("  -> pid %d (%s) exited with code %d\n", r.pid, r.name, r.exit_code);
    }
    /* A fault was already reported by the kernel as it happened. */
}

void lesson_usermode(void) {
    learn_page("User mode: running untrusted code");
    learn_heading("Why a kernel needs rings at all");
    learn_para("Until now every line of code ran in ring 0 and could do "
               "anything. A real system runs programs you did not write, and "
               "one bug in them must not take down the machine or read "
               "another program's data. The CPU supports this with privilege "
               "rings; the kernel's job is to set up four things so that ring "
               "3 is genuinely walled in:");
    kprintf("  1. segments    GDT entries with DPL 3 for user code and data\n");
    kprintf("  2. a TSS       tells the CPU which stack to use when ring 3 traps in\n");
    kprintf("  3. page bits   the U/S bit: only pages marked user are reachable\n");
    kprintf("  4. one gate    the IDT gate for int 0x80 has DPL 3; all others DPL 0\n\n");
    learn_para("Getting INTO ring 3 is odd: no instruction means 'drop "
               "privilege'. Instead the kernel builds the stack frame an "
               "interrupt from ring 3 would have pushed (SS, RSP, RFLAGS, "
               "CS, RIP) and executes iretq to 'return' there. See "
               "src/proc/usermode.asm - it is twelve lines.");
    LEARN_NEXT();

    struct dtr gdtr;

    sgdt(&gdtr);
    const uint64_t *gdt = (const uint64_t *)(uintptr_t)gdtr.base;

    learn_page("User mode: running untrusted code");
    learn_live("the GDT and TSS that make ring 3 possible");
    kprintf("  idx sel   access  meaning\n");
    kprintf("   1  0x%02x   0x%02lx    kernel code, DPL 0\n", GDT_KERNEL_CODE, (gdt[1] >> 40) & 0xFF);
    kprintf("   2  0x%02x   0x%02lx    kernel data, DPL 0\n", GDT_KERNEL_DATA, (gdt[2] >> 40) & 0xFF);
    kprintf("   3  0x%02x   0x%02lx    user data,   DPL 3   (selector 0x18 | RPL 3)\n", GDT_USER_DATA,
            (gdt[3] >> 40) & 0xFF);
    kprintf("   4  0x%02x   0x%02lx    user code,   DPL 3   (selector 0x20 | RPL 3), L=1\n", GDT_USER_CODE,
            (gdt[4] >> 40) & 0xFF);
    kprintf("  5-6 0x%02x   0x%02lx    64-bit TSS (busy: ltr loaded it), 16-byte descriptor\n", GDT_TSS,
            (gdt[5] >> 40) & 0xFF);
    kprintf("  TSS at 0x%lx, rsp0 = 0x%lx\n", gdt_tss_address(), gdt_get_kernel_stack());
    kprintf("  rsp0: where the CPU switches when ring 3 traps in; the scheduler\n");
    kprintf("  rewrites it on every switch so each process has its own.\n\n");
    learn_para("The low two bits of a selector are the privilege it asks for: "
               "0x23 and 0x1B end in 3, ring 3. The CPU checks that against the "
               "descriptor's DPL on every segment load.");
    LEARN_NEXT();

    learn_page("User mode: running untrusted code");
    learn_live("running user/prog_hello.c in ring 3");
    uint64_t before = interrupt_count(SYSCALL_VECTOR);

    run_and_report("hello");
    kprintf("\n  int 0x80 fired %lu times for this run (each write/getpid/exit)\n\n",
            interrupt_count(SYSCALL_VECTOR) - before);
    learn_para("That text came from a program running in ring 3: it read its "
               "own CS register (0x23, ending in binary 11) and could only "
               "talk to the screen by asking the kernel through int 0x80. Its "
               "code was loaded from an ELF file into a private address space "
               "(next two lessons) and entered with iretq.");
    learn_try("run hello    progs    ps (a user process shows mode 'user')");
    LEARN_NEXT();

    learn_page("User mode: running untrusted code");
    learn_live("what happens when a user program misbehaves");
    run_and_report("privileged");
    run_and_report("crash_kernel");
    run_and_report("crash_unmapped");
    kprintf("\n");
    learn_para("Three different walls. 'cli' is a privileged instruction: ring "
               "3 gets a general protection fault. Reading 0x100000 hits a "
               "page that exists but has no U bit: a protection violation. "
               "Writing 0x80000000 hits a page that is simply not mapped. "
               "In each case the CPU enters the kernel through the exception "
               "gate; process_fault() sees the saved CS was ring 3, blames the "
               "program, and ends that one process. Compare a fault in kernel "
               "code: the whole system halts.");
    learn_try("spawn spin   ps   kill <pid>   - a runaway loop is preempted, the shell lives");
}
