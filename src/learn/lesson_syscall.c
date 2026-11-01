#include "learn/learn.h"
#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "lib/kprintf.h"
#include "syscall/syscall.h"

void lesson_syscall(void) {
    learn_page("System calls");
    learn_heading("The only door into the kernel");
    learn_para("A program in ring 3 cannot call a kernel function: the kernel's "
               "memory is off limits to it, and so are privileged "
               "instructions. Yet it must be able to ask for a file, more "
               "memory, or the time. A system call is the sanctioned way: an "
               "instruction that deliberately traps into the kernel through "
               "an entry point the kernel chose.");
    learn_para("On x86_64 that instruction is usually syscall; the classic "
               "form is a software interrupt, int 0x80, which is what we use "
               "because everything you learned about the IDT applies. The CPU "
               "switches to ring 0 and to a kernel stack, saves the caller's "
               "state, and runs the handler. The handler reads the call "
               "number and arguments from registers, validates them, "
               "dispatches through a table, and puts the result in rax. "
               "iretq returns to ring 3.");
    learn_para("Honest caveat: KernelCraft has no user mode yet. The caller "
               "here is kernel code already in ring 0, so the CPU does not "
               "change privilege or stack. Everything else is genuine: the "
               "same gate, register ABI, table and return path.");
    LEARN_NEXT();

    learn_page("System calls");
    learn_live("making real system calls with int 0x80");
    uint64_t before = interrupt_count(SYSCALL_VECTOR);
    static const char msg[] = "hello from inside a system call\n";
    long written = syscall3(SYS_WRITE, (long)(uintptr_t)msg, (long)(sizeof(msg) - 1), 0);

    kprintf("  write returned %ld\n", written);
    kprintf("  getpid()     = %ld   (the id of this thread)\n", syscall3(SYS_GETPID, 0, 0, 0));
    kprintf("  uptime_ms()  = %ld\n", syscall3(SYS_UPTIME_MS, 0, 0, 0));
    kprintf("  yield()      = %ld\n", syscall3(SYS_YIELD, 0, 0, 0));
    kprintf("  sleep_ms(50) = %ld\n", syscall3(SYS_SLEEP_MS, 50, 0, 0));
    kprintf("  call 99      = %ld   (no such call: -1)\n", syscall3(99, 0, 0, 0));
    kprintf("  write(bad pointer) = %ld   (rejected, not a crash)\n",
            syscall3(SYS_WRITE, 0x7FFFFFFFFFFFLL, 16, 0));
    kprintf("\n  interrupt 0x80 fired %lu times during this page\n\n",
            interrupt_count(SYSCALL_VECTOR) - before);
    kprintf("  number name       arguments\n");
    for (uint64_t i = 0; i < SYSCALL_COUNT; i++) {
        kprintf("  %5lu  %-10s %s\n", i, syscall_name(i),
                i == SYS_WRITE ? "rdi = buffer, rsi = length" :
                i == SYS_SLEEP_MS ? "rdi = milliseconds" : "none");
    }
    kprintf("\n");
    learn_try("learn idt  (vector 128 is now a present gate, installed by syscall_init)");
    LEARN_NEXT();

    learn_page("System calls");
    learn_heading("The hard part is distrust");
    learn_para("Look at the 'bad pointer' line. A kernel must treat every "
               "argument from a program as hostile: a pointer might aim at "
               "kernel memory, wrap around the address space, or reach "
               "unmapped pages. syscall.c checks that the whole range lies "
               "where it is allowed before touching it; real kernels do the "
               "same with copy_from_user / copy_to_user, and many past "
               "vulnerabilities were a missing check exactly here.");
    learn_para("To turn this into real system calls you need four things, "
               "all covered in earlier lessons: user code and data segments in "
               "the GDT (DPL 3), a TSS so the CPU knows the kernel stack, page "
               "tables with the US bit set for user memory only, and an IDT "
               "gate with DPL 3 (attribute 0xEE) so int 0x80 is legal from "
               "ring 3. Linux skips the IDT for speed and uses the syscall "
               "instruction, configured through MSRs.");
    learn_try("exercise: add SYS_GETTICKS to syscall.h/.c and call it from a "
              "thread.");
}
