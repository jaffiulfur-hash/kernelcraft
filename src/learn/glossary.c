#include "learn/learn.h"
#include "lib/kprintf.h"
#include "lib/string.h"

struct term {
    const char *name;
    const char *meaning;
};

/* Keep alphabetical: `glossary` prints the table in this order. */
static const struct term terms[] = {
    { "abi", "Application Binary Interface: the rules for how compiled code calls other code - which registers carry arguments, who saves what. We follow the System V AMD64 ABI." },
    { "address space", "The set of addresses a program can use. With paging each process can have its own, so one program cannot see another's memory." },
    { "apic", "Advanced Programmable Interrupt Controller: the modern replacement for the 8259 PIC, one per CPU core plus I/O APICs, needed for multiple CPUs." },
    { "atomic", "An operation that cannot be interrupted halfway, as seen by other code. On x86, instructions with the lock prefix or xchg are atomic." },
    { "bitmap", "An array where each bit stands for one thing. Our PMM uses one bit per 4 KiB frame: 1 = in use, 0 = free." },
    { "bootloader", "The program that loads the kernel into memory and starts it. Here: GRUB, speaking the Multiboot2 protocol." },
    { "bss", "The section of an executable for zero-initialised data. The file stores only its size; the loader or kernel provides the zeroed memory." },
    { "context switch", "Saving one thread's CPU state and loading another's so the CPU continues the other thread. See sched/switch.asm." },
    { "cr3", "Control register holding the physical address of the top-level page table. Writing it switches address spaces and flushes the TLB." },
    { "critical section", "A stretch of code that must not run concurrently with another touching the same data. On one CPU, disabling interrupts is enough." },
    { "deadlock", "Two or more threads each waiting for something the other holds, so none can ever continue." },
    { "dpl", "Descriptor Privilege Level: the ring (0-3) a GDT or IDT entry belongs to or requires." },
    { "elf", "Executable and Linkable Format: the file format of programs. A header plus program headers say which bytes to load at which virtual addresses with which permissions, and where execution starts." },
    { "eoi", "End Of Interrupt: a command to the PIC that says the handler is done. Without it the PIC delivers no more interrupts on that line." },
    { "exception", "A synchronous interrupt raised by the CPU itself: divide error, invalid opcode, page fault. Vectors 0-31." },
    { "fault", "An exception reported before the instruction completes, so it can be fixed and retried. Page faults are the classic example." },
    { "frame", "A fixed-size chunk of physical memory (4 KiB here). Pages are virtual; frames are physical." },
    { "gdt", "Global Descriptor Table: the table of segment descriptors. In long mode it mostly defines privilege rings and the 64-bit code segment." },
    { "heap", "Memory handed out in variable-sized pieces at run time by kmalloc/kfree. Not to be confused with the heap data structure." },
    { "identity mapping", "A page-table mapping where virtual address = physical address. boot.asm sets one up for the first 1 GiB." },
    { "idt", "Interrupt Descriptor Table: 256 gates telling the CPU which handler to run for each interrupt vector." },
    { "interrupt", "A signal that makes the CPU stop what it is doing and run a handler: from hardware (IRQ), from the CPU (exception) or from software (int n)." },
    { "iretq", "The instruction that returns from an interrupt by popping RIP, CS, RFLAGS, RSP and SS. The kernel also uses it to enter ring 3 for the first time, by building the frame by hand." },
    { "irq", "Interrupt ReQuest: a hardware interrupt line, e.g. IRQ 0 is the timer and IRQ 1 the keyboard." },
    { "isr", "Interrupt Service Routine: the handler for an interrupt. Ours are the stubs in isr.asm and irq.asm that call interrupt_dispatch()." },
    { "kernel", "The core of the operating system: the part that runs in ring 0 and controls CPU, memory and devices." },
    { "long mode", "The 64-bit operating mode of x86_64 processors. Entered by setting PAE, EFER.LME and then paging." },
    { "mmu", "Memory Management Unit: the CPU hardware that translates virtual addresses to physical ones using the page tables." },
    { "multiboot2", "A standard protocol between bootloaders and kernels. The kernel carries a magic header; the loader leaves a tagged information block." },
    { "mutex", "Mutual-exclusion lock: only one thread at a time may hold it, others wait." },
    { "nx", "No-execute: a page-table bit (bit 63, honoured once EFER.NXE is set) that makes a page non-executable. User data and stack pages carry it, so injected data cannot run as code." },
    { "page", "A fixed-size block of virtual address space (4 KiB, or 2 MiB for huge pages). The unit the MMU maps." },
    { "page fault", "Exception 14, raised when an access hits an unmapped page or violates its permissions. CR2 holds the address." },
    { "page table", "The in-memory structure the MMU walks to translate addresses: four levels of 512 entries on x86_64." },
    { "pci", "Peripheral Component Interconnect: the bus most devices sit on. Software discovers devices by reading each one's configuration space." },
    { "pic", "Programmable Interrupt Controller (8259): routes device IRQ lines to the CPU. Two chained chips give 16 IRQs." },
    { "pit", "Programmable Interval Timer (8253/8254): divides a 1.19 MHz clock to produce IRQ 0 at a chosen rate." },
    { "preemption", "Taking the CPU away from a running thread, typically on a timer interrupt, so others get a turn." },
    { "process", "A running program: one or more threads plus a private address space. Here a process is a kernel thread with its own page tables and a ring 3 side." },
    { "quantum", "The time slice a thread may run before the scheduler may preempt it. Ours is a few timer ticks." },
    { "race condition", "A bug where the result depends on the unlucky timing of two threads touching shared data." },
    { "ring", "A privilege level. Ring 0 is the kernel, ring 3 is ordinary programs; x86 also defines rings 1 and 2, rarely used." },
    { "scheduler", "The part of the kernel that decides which thread runs next." },
    { "segment", "A region of memory described by a GDT entry. In long mode base and limit are ignored for code and data." },
    { "spinlock", "A lock where a waiter loops until it can take it. Fine for very short critical sections." },
    { "syscall", "A request from a program to the kernel, crossing from ring 3 to ring 0 through a controlled gate (int 0x80 or the syscall instruction)." },
    { "thread", "A unit of execution: its own stack and saved registers, sharing memory with others in the same process." },
    { "tlb", "Translation Lookaside Buffer: a cache of recent page-table translations. Must be flushed when mappings change." },
    { "trap", "An exception reported after the instruction completes, so execution resumes after it. int3 is a trap." },
    { "triple fault", "A fault while handling a double fault. The CPU gives up and resets the machine." },
    { "tss", "Task State Segment: in long mode mainly holds the kernel stack pointer used when entering ring 0 from ring 3." },
    { "user mode", "Ring 3: the restricted privilege level programs run in. It cannot run privileged instructions or touch kernel pages, and reaches the kernel only through gates such as int 0x80." },
    { "vga text mode", "The legacy 80x25 text display. Memory at 0xB8000 holds 2 bytes per cell: a character and a colour attribute." },
    { "virtual address", "An address used by a program, translated by the MMU into a physical address." },
};

#define TERM_COUNT LEARN_COUNT(terms)

int glossary_term_count(void) {
    return TERM_COUNT;
}

const char *glossary_lookup(const char *term) {
    for (int i = 0; i < (int)TERM_COUNT; i++) {
        if (strcasecmp(term, terms[i].name) == 0) {
            return terms[i].meaning;
        }
    }

    return 0;
}

static int starts_with_nocase(const char *s, const char *prefix) {
    return strncasecmp(s, prefix, strlen(prefix)) == 0;
}

void glossary_command(int argc, char **argv) {
    if (argc == 1) {
        kprintf("%d terms - 'glossary <term>' for a definition:\n", (int)TERM_COUNT);
        for (int i = 0; i < (int)TERM_COUNT; i++) {
            kprintf("%-19s%s", terms[i].name, (i % 4 == 3 || i == (int)TERM_COUNT - 1) ? "\n" : "");
        }
        return;
    }

    /* Join the words: "glossary page fault" looks up "page fault". */
    char query[48];
    size_t len = 0;

    query[0] = '\0';
    for (int i = 1; i < argc; i++) {
        for (const char *p = argv[i]; *p && len < sizeof(query) - 2; p++) {
            query[len++] = *p;
        }
        if (i + 1 < argc && len < sizeof(query) - 2) {
            query[len++] = ' ';
        }
    }
    query[len] = '\0';

    const char *meaning = glossary_lookup(query);

    if (meaning) {
        learn_heading(query);
        learn_para(meaning);
        return;
    }

    kprintf("no entry for '%s'.", query);
    int shown = 0;
    for (int i = 0; i < (int)TERM_COUNT; i++) {
        if (starts_with_nocase(terms[i].name, query)) {
            kprintf("%s%s", shown++ ? ", " : " Did you mean: ", terms[i].name);
        }
    }
    kprintf("\n");
}
