#include "learn/learn.h"
#include "arch/keyboard.h"
#include "arch/pit.h"
#include "lib/kprintf.h"
#include "vga.h"

struct question {
    const char *text;
    const char *choice[4];
    int answer;                 /* index of the correct choice */
    const char *why;
};

static const struct question questions[] = {
    { "Which register holds the physical address of the top-level page table?",
      { "CR0", "CR2", "CR3", "CR4" }, 2,
      "CR3. Writing it switches address spaces. CR2 holds the faulting address after a page fault." },
    { "A device raised an IRQ and our handler ran. What does the End Of Interrupt (EOI) do?",
      { "Clears the device's data", "Tells the PIC it may deliver more interrupts on that line",
        "Re-enables paging", "Resets the CPU" }, 1,
      "Until the PIC sees EOI it will not deliver another interrupt of that priority or lower, so forgetting it freezes the timer." },
    { "Why is the PIC remapped to vectors 32-47?",
      { "To make it faster", "Its default vectors 8-15 collide with CPU exceptions",
        "The IDT only has 32 entries", "Because 32 is a power of two" }, 1,
      "By default IRQ 0-7 land on vectors 8-15, where exception 8 is the double fault: a timer tick would look like a crash." },
    { "Which exception is raised by touching an unmapped page?",
      { "General Protection Fault (13)", "Page Fault (14)", "Invalid Opcode (6)", "Divide Error (0)" }, 1,
      "Vector 14. CR2 holds the address and the error code says why (not present, write, user mode...)." },
    { "In 64-bit long mode, what do a segment's base and limit do for ordinary code and data?",
      { "Select the page", "They are ignored: memory is flat", "They limit the stack", "They set the privilege" }, 1,
      "Base and limit are ignored; the GDT still matters for privilege levels and the 64-bit code (L) bit." },
    { "What is the difference between a fault and a trap?",
      { "Faults are hardware, traps are software", "A fault is reported before the instruction completes so it can be retried; a trap after",
        "Traps cannot be handled", "There is none" }, 1,
      "A page fault can be fixed and the instruction re-run. A breakpoint trap resumes after the int3." },
    { "Which registers must context_switch save on x86_64 System V?",
      { "All sixteen", "Only rax and rdx", "rbx, rbp, r12-r15 (callee-saved) plus rsp", "None" }, 2,
      "The caller already saved what it needs; the callee-saved registers and the stack pointer are all that carries over." },
    { "A race condition is a bug where...",
      { "the CPU runs too fast", "the result depends on the timing of threads touching shared data",
        "memory runs out", "a driver is missing" }, 1,
      "counter++ is load, add, store; if another thread runs between load and store an update is lost." },
    { "First-fit allocation hands out...",
      { "the smallest block that fits", "the largest block", "the first block that is big enough", "a random block" }, 2,
      "First fit stops at the first big-enough block. Best fit searches for the tightest one." },
    { "One bit per 4 KiB frame: how many bytes of bitmap track 128 MiB?",
      { "1 KiB", "4 KiB", "32 KiB", "128 KiB" }, 1,
      "128 MiB / 4 KiB = 32768 frames = 32768 bits = 4096 bytes." },
    { "What does coalescing do in a heap allocator?",
      { "Compresses data", "Merges adjacent free blocks", "Zeroes freed memory", "Sorts the blocks" }, 1,
      "Without it, freeing many small blocks leaves unusable crumbs instead of one large free block." },
    { "What does the magic number 0x36D76289 in EAX at boot tell the kernel?",
      { "The CPU is 64-bit", "It was loaded by a Multiboot2 bootloader", "Paging is on", "The disk is ready" }, 1,
      "It is the Multiboot2 bootloader magic. EBX then holds the address of the boot information block." },
    { "Which privilege ring does the kernel normally run in?",
      { "Ring 0", "Ring 1", "Ring 2", "Ring 3" }, 0,
      "Ring 0 is the most privileged. Ordinary programs run in ring 3." },
    { "Why must the IRQ 1 handler read port 0x60?",
      { "To clear the screen", "Otherwise the keyboard controller sends no more bytes",
        "To set the LEDs", "To flush the TLB" }, 1,
      "The scancode sits in the controller's output buffer until read; while it is full, no further key is reported." },
    { "What is preemption?",
      { "Running code before main", "The kernel forcibly taking the CPU from a running thread, usually on a timer tick",
        "Pre-allocating memory", "Compiling ahead of time" }, 1,
      "Without it one thread that never yields could hog the machine." },
    { "Which x86 instruction makes test-and-set atomic for a lock?",
      { "mov", "xchg (with a memory operand)", "nop", "hlt" }, 1,
      "xchg with memory is implicitly locked: no other CPU or interrupt can slip in between its read and write." },
    { "What does an identity mapping mean?",
      { "Pages are encrypted", "Virtual address equals physical address", "Each process has an ID", "Only the kernel is mapped" }, 1,
      "boot.asm identity-maps the first 1 GiB, so a pointer and a physical address are the same number." },
    { "Why can a user program not just call a kernel function?",
      { "It is too slow", "Kernel memory and privileged instructions are protected, so it must enter through a system call gate",
        "Functions cannot be shared", "The linker forbids it" }, 1,
      "The gate lets the kernel control exactly where and how it is entered." },
    { "How many bits of a virtual address select the entry at each page-table level?",
      { "4", "9", "12", "16" }, 1,
      "Each table is 4 KiB of 8-byte entries = 512 entries = 9 bits. 4 x 9 + 12 offset bits = 48." },
    { "Why must the IDT be loaded before interrupts are enabled with sti?",
      { "sti is slow without it", "An interrupt with no valid gate cannot even be reported: the CPU escalates to a double and then triple fault",
        "The PIC needs it", "The keyboard needs it" }, 1,
      "With no handler the CPU cannot deliver the resulting fault either, so it resets the machine. kmain loads the GDT and IDT before it runs sti." },
    { "What does the U/S bit in a page-table entry control?",
      { "Whether the page is swapped", "Whether code in ring 3 may access the page", "Whether the page is shared", "The page size" }, 1,
      "A page is reachable from ring 3 only if the U/S bit is set at every level of the walk. Kernel pages lack it." },
    { "Why does the CPU need the TSS when a ring 3 program makes a system call?",
      { "To count system calls", "To find the kernel stack (rsp0) to switch to", "To flush the TLB", "To load the program" }, 1,
      "The user stack is under the program's control, so the CPU must switch to a stack the kernel chose before running kernel code." },
    { "How does the kernel start a program in ring 3 for the first time?",
      { "With a jmp", "By building an interrupt-return frame by hand and executing iretq", "With the hlt instruction", "By rebooting" }, 1,
      "No instruction lowers privilege directly; iretq with a CS of ring 3 is how the CPU drops to it." },
};

#define QUESTION_COUNT ((int)(sizeof(questions) / sizeof(questions[0])))
#define QUIZ_ROUNDS 10

int quiz_question_count(void) {
    return QUESTION_COUNT;
}

/* Every question needs four non-empty choices, a valid answer and an explanation. */
int quiz_validate(void) {
    for (int i = 0; i < QUESTION_COUNT; i++) {
        const struct question *q = &questions[i];

        if (!q->text || !q->why || q->answer < 0 || q->answer > 3) {
            return -1;
        }
        for (int c = 0; c < 4; c++) {
            if (!q->choice[c] || !q->choice[c][0]) {
                return -1;
            }
        }
    }

    return 0;
}

static uint32_t rng_state;

static uint32_t next_random(void) {
    rng_state = rng_state * 1664525u + 1013904223u;    /* classic LCG */
    return rng_state >> 8;
}

static void wrap_print(const char *text, int first_col, int indent);

void quiz_command(int argc, char **argv) {
    int order[QUESTION_COUNT];
    int score = 0, asked = 0;

    (void)argc;
    (void)argv;

    /* Seed from the timer, then shuffle (Fisher-Yates) so every run differs. */
    rng_state = (uint32_t)pit_get_ticks() * 2654435761u + 12345u;
    for (int i = 0; i < QUESTION_COUNT; i++) {
        order[i] = i;
    }
    for (int i = QUESTION_COUNT - 1; i > 0; i--) {
        int j = (int)(next_random() % (uint32_t)(i + 1));
        int tmp = order[i];

        order[i] = order[j];
        order[j] = tmp;
    }

    for (int round = 0; round < QUIZ_ROUNDS; round++) {
        const struct question *q = &questions[order[round]];

        vga_clear();
        vga_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
        kprintf(" OS quiz: question %d of %d     score %d ", round + 1, QUIZ_ROUNDS, score);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        kprintf("\n\n");
        wrap_print(q->text, 0, 0);
        kprintf("\n\n");
        for (int c = 0; c < 4; c++) {
            kprintf("  %c) ", 'a' + c);
            wrap_print(q->choice[c], 5, 5);
            kprintf("\n");
        }
        kprintf("\nYour answer (a-d, q to stop): ");

        int pick = -1;

        for (;;) {
            char ch = keyboard_getc();

            if (ch == 'q' || ch == 'Q') {
                pick = -2;
                break;
            }
            if (ch >= 'a' && ch <= 'd') {
                pick = ch - 'a';
                break;
            }
            if (ch >= 'A' && ch <= 'D') {
                pick = ch - 'A';
                break;
            }
        }

        if (pick == -2) {
            kprintf("\n");
            break;
        }

        asked++;
        kprintf("%c\n\n", 'a' + pick);
        if (pick == q->answer) {
            score++;
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            kprintf("Correct!\n");
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            kprintf("Not quite. The answer is %c)\n  ", 'a' + q->answer);
            wrap_print(q->choice[q->answer], 2, 2);
            kprintf("\n");
        }
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        wrap_print(q->why, 0, 0);
        kprintf("\n\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        kprintf("-- Enter: continue --");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        while (keyboard_getc() != '\n') {
        }
    }

    vga_clear();
    kprintf("Quiz finished: %d of %d correct.\n", score, asked);
    if (asked == 0) {
        kprintf("Run 'quiz' again when you are ready.\n");
    } else if (score * 10 >= asked * 9) {
        kprintf("Excellent - you could write the next stage yourself.\n");
    } else if (score * 10 >= asked * 6) {
        kprintf("Good. Revisit the lessons you missed: 'learn'.\n");
    } else {
        kprintf("Keep going - 'learn' walks through each topic with live data.\n");
    }
}

/* Same word wrap as the lessons, kept local so quiz text can share a line. */
static void wrap_print(const char *text, int first_col, int indent) {
    int col = first_col;

    while (*text) {
        int len = 0;

        while (text[len] && text[len] != ' ') {
            len++;
        }
        if (col > indent && col + 1 + len > 76) {
            kprintf("\n");
            for (col = 0; col < indent; col++) {
                kprintf(" ");
            }
        } else if (col > indent) {
            kprintf(" ");
            col++;
        }
        for (int i = 0; i < len; i++) {
            kprintf("%c", text[i]);
        }
        col += len;
        text += len;
        while (*text == ' ') {
            text++;
        }
    }
}
