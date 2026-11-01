#include "learn/learn.h"
#include "arch/keyboard.h"
#include "arch/log.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "vga.h"

#define TEXT_WIDTH 76

static int auto_mode;
static int page_number;
static int lesson_number;

static const struct lesson lessons[] = {
    { "roadmap",  "How an operating system is built",
      "the big picture and the order to build things in", lesson_roadmap },
    { "boot",     "From power button to kmain",
      "firmware, GRUB, Multiboot2 and the jump to 64-bit mode", lesson_boot },
    { "cpu",      "The CPU: modes, registers and CPUID",
      "what the processor is and what state it is in right now", lesson_cpu },
    { "gdt",      "Segmentation and the GDT",
      "decode the live Global Descriptor Table", lesson_gdt },
    { "idt",      "Exceptions and the IDT",
      "how the CPU finds our code when something happens", lesson_idt },
    { "irq",      "Hardware interrupts, the PIC and the timer",
      "IRQs, EOI, the PIT, and measuring a tick", lesson_irq },
    { "keyboard", "From keypress to character",
      "watch raw scancodes arrive", lesson_keyboard },
    { "pmm",      "Physical memory",
      "a live map of RAM; first-fit allocation and fragmentation", lesson_pmm },
    { "paging",   "Virtual memory and page tables",
      "walk the real page tables like the MMU does", lesson_paging },
    { "heap",     "Dynamic memory: the kernel heap",
      "watch kmalloc split and kfree coalesce, drawn live", lesson_heap },
    { "sched",    "Threads and the scheduler",
      "context switches and preemption, with live threads", lesson_sched },
    { "sync",     "Races and locks",
      "lose updates on purpose, then fix them with a mutex", lesson_sync },
};

#define LESSON_COUNT ((int)(sizeof(lessons) / sizeof(lessons[0])))

int learn_lesson_count(void) {
    return LESSON_COUNT;
}

int learn_is_auto(void) {
    return auto_mode;
}

/* ---- page layout ------------------------------------------------------ */

void learn_page(const char *title) {
    page_number++;

    if (auto_mode) {
        kprintf("\n");
    } else {
        vga_clear();
    }

    vga_set_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_CYAN);
    kprintf(" Lesson %d: %-48s page %-2d ", lesson_number, title, page_number);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("\n\n");
}

void learn_heading(const char *text) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("%s\n", text);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void learn_live(const char *what) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("[LIVE] %s\n", what);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/*
 * Word-wrap `text` to TEXT_WIDTH columns. The first line starts at column
 * `first_col` (the caller already printed a prefix); continuation lines are
 * indented by `indent` spaces. '\n' in the text forces a line break.
 */
static void wrap_text(const char *text, int first_col, int indent) {
    int col = first_col;

    while (*text) {
        if (*text == '\n') {
            log_write_char('\n');
            for (col = 0; col < indent; col++) {
                log_write_char(' ');
            }
            text++;
            continue;
        }

        if (*text == ' ') {
            text++;
            continue;
        }

        int len = 0;
        while (text[len] && text[len] != ' ' && text[len] != '\n') {
            len++;
        }

        if (col > indent && col + 1 + len > TEXT_WIDTH) {
            log_write_char('\n');
            for (col = 0; col < indent; col++) {
                log_write_char(' ');
            }
        }
        if (col > indent) {
            log_write_char(' ');
            col++;
        }

        for (int i = 0; i < len; i++) {
            log_write_char(text[i]);
        }
        col += len;
        text += len;
    }
}

void learn_try(const char *text) {
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    kprintf("TRY: ");
    wrap_text(text, 5, 5);
    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

void learn_para(const char *text) {
    wrap_text(text, 0, 0);
    kprintf("\n\n");
}

int learn_next(void) {
    if (auto_mode) {
        return 1;
    }

    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    kprintf("-- Enter: next page    q: quit lesson --");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (;;) {
        char c = keyboard_getc();

        if (c == '\n') {
            return 1;
        }
        if (c == 'q' || c == 'Q') {
            kprintf("\n");
            return 0;
        }
    }
}

void learn_print_flags(uint64_t value, const struct learn_flag *flags, int count) {
    int printed = 0;

    for (int i = 0; i < count; i++) {
        if (value & (1ull << flags[i].bit)) {
            kprintf("%s%s", printed ? " " : "", flags[i].name);
            printed++;
        }
    }

    if (!printed) {
        kprintf("(none)");
    }
}

/* ---- running lessons -------------------------------------------------- */

static void run_lesson(int index) {
    lesson_number = index + 1;
    page_number = 0;

    lessons[index].run();

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("-- end of lesson %d (%s). Type 'learn' for the list --\n",
            lesson_number, lessons[index].name);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static int find_lesson(const char *key) {
    uint64_t n;

    if (str_to_u64(key, &n) == 0 && n >= 1 && n <= (uint64_t)LESSON_COUNT) {
        return (int)n - 1;
    }

    for (int i = 0; i < LESSON_COUNT; i++) {
        if (strcasecmp(key, lessons[i].name) == 0) {
            return i;
        }
    }

    return -1;
}

static void print_index(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("KernelCraft curriculum - run a lesson with: learn <number|name>\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (int i = 0; i < LESSON_COUNT; i++) {
        kprintf("  %2d  %-9s %s\n", i + 1, lessons[i].name, lessons[i].summary);
    }

    kprintf("also: glossary [term]\n");
}

void learn_command(int argc, char **argv) {
    if (argc == 1) {
        print_index();
        return;
    }

    int idx = find_lesson(argv[1]);

    if (idx < 0) {
        kprintf("no such lesson: %s (try 'learn')\n", argv[1]);
        return;
    }

    if (argc > 2 && strcmp(argv[2], "--auto") == 0) {
        auto_mode = 1;
        run_lesson(idx);
        auto_mode = 0;
        return;
    }

    run_lesson(idx);
}

void learn_run_all_auto(void) {
    auto_mode = 1;
    for (int i = 0; i < LESSON_COUNT; i++) {
        run_lesson(i);
    }
    auto_mode = 0;
}
