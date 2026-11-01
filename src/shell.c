#include "shell.h"
#include "kernel.h"
#include "layout.h"
#include "learn/learn.h"
#include "panic.h"
#include "selftest.h"
#include "vga.h"
#include "arch/debug.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "arch/rtc.h"
#include "arch/power.h"
#include "arch/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "sched/thread.h"
#include "mm/paging.h"
#include "mm/pmm.h"

#define SHELL_BUFFER_SIZE 128
#define SHELL_MAX_ARGS    8
#define HEXDUMP_MAX_BYTES 512

static char shell_buffer[SHELL_BUFFER_SIZE];
static int shell_length = 0;

/*
 * Commands are described by a table at the bottom of this file, so adding one
 * means writing a handler and adding one line. `help` is generated from it.
 *
 * Handlers get argv[0] = command name, like main() in a hosted program.
 * Output convention: print complete lines, each ending in '\n'.
 */
typedef void (*shell_handler_t)(int argc, char **argv);

struct shell_command {
    const char *name;
    const char *usage;     /* shown by help, e.g. "alloc [frames]" */
    const char *help;
    shell_handler_t run;
};

/* ---- helpers ---------------------------------------------------------- */

static int parse_arg(const char *text, const char *what, uint64_t *out) {
    if (str_to_u64(text, out) != 0) {
        kprintf("invalid %s: %s\n", what, text);
        return -1;
    }

    return 0;
}

static void print_usage(const char *usage) {
    kprintf("usage: %s\n", usage);
}

static void trigger_breakpoint(void) {
    __asm__ volatile ("int3");
}

static void trigger_divide_by_zero(void) {
    volatile unsigned long x = 1;
    volatile unsigned long y = 0;
    volatile unsigned long z = x / y;
    (void)z;
}

/* ---- general commands ------------------------------------------------- */

static void cmd_help(int argc, char **argv);

static void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    vga_clear();
}

static void cmd_about(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("%s - educational kernel\n", KERNELCRAFT_NAME);
    kprintf("stage %s - %s\n", KERNELCRAFT_STAGE, KERNELCRAFT_STAGE_TITLE);
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", i > 1 ? " " : "", argv[i]);
    }
    kprintf("\n");
}

static void cmd_ticks(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("ticks=%lu\n", pit_get_ticks());
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;

    uint64_t ticks = pit_get_ticks();

    kprintf("up %lu.%02lu seconds (%lu ticks at %d Hz)\n", ticks / PIT_HZ,
            (ticks % PIT_HZ) * 100 / PIT_HZ, ticks, PIT_HZ);
}

static void cmd_date(int argc, char **argv) {
    struct rtc_time t;

    (void)argc;
    (void)argv;
    rtc_read(&t);
    kprintf("%04d-%02d-%02d %02d:%02d:%02d (RTC, usually UTC)\n", t.year, t.month,
            t.day, t.hour, t.minute, t.second);
}

static void cmd_sleep(int argc, char **argv) {
    uint64_t ms;

    if (argc != 2) {
        print_usage("sleep <milliseconds>");
        return;
    }
    if (parse_arg(argv[1], "milliseconds", &ms) != 0) {
        return;
    }

    thread_sleep_ticks((ms * PIT_HZ + 999) / 1000);
}

static void cmd_ps(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("  id name             state     cpu ticks  switches\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        const struct thread *t = sched_thread_at(i);

        if (t) {
            kprintf("  %2d %-16s %-9s %9lu  %8lu\n", t->id, t->name,
                    thread_state_name(t->state), t->cpu_ticks, t->switches);
        }
    }
    kprintf("context switches so far: %lu   preemption: %s   quantum: %d ticks\n",
            sched_switch_count(), sched_preemption_enabled() ? "on" : "off",
            THREAD_QUANTUM_TICKS);
}

static void cmd_vtop(int argc, char **argv) {
    uint64_t addr;
    struct paging_walk w;

    if (argc != 2) {
        print_usage("vtop <virtual address>");
        return;
    }
    if (parse_arg(argv[1], "address", &addr) != 0) {
        return;
    }

    paging_walk(addr, &w);
    paging_print_walk(&w);
}

static void cmd_learn(int argc, char **argv) {
    learn_command(argc, argv);
}

static void cmd_glossary(int argc, char **argv) {
    glossary_command(argc, argv);
}

static void cmd_pf(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("reading unmapped address 0x40000000 (just past the 1 GiB identity map)...\n");
    volatile uint64_t *p = (volatile uint64_t *)0x40000000ull;
    uint64_t v = *p;
    (void)v;
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("rebooting...\n");
    arch_reboot();
}

static void cmd_halt(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("system halted\n");
    arch_halt();
}

/* ---- debugging commands ----------------------------------------------- */

static void cmd_panic(int argc, char **argv) {
    (void)argc;
    (void)argv;
    panic("panic command invoked");
}

static void cmd_int3(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("triggering breakpoint...\n");
    trigger_breakpoint();
    kprintf("returned from breakpoint\n");
}

static void cmd_div0(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("triggering divide by zero...\n");
    trigger_divide_by_zero();
}

static void cmd_regs(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (debug_has_last_frame()) {
        debug_print_frame(debug_get_last_frame());
    } else {
        kprintf("no saved interrupt frame\n");
    }
}

static void cmd_hexdump(int argc, char **argv) {
    uint64_t addr;
    uint64_t len = 64;

    if (argc < 2 || argc > 3) {
        print_usage("hexdump <addr> [len]");
        return;
    }

    if (parse_arg(argv[1], "address", &addr) != 0) {
        return;
    }

    if (argc == 3 && parse_arg(argv[2], "length", &len) != 0) {
        return;
    }

    if (len == 0 || len > HEXDUMP_MAX_BYTES) {
        kprintf("length must be 1..%d\n", HEXDUMP_MAX_BYTES);
        return;
    }

    /* Only the first GiB is mapped; touching anything else would page fault. */
    if (addr >= BOOT_IDENTITY_MAP_BYTES || len > BOOT_IDENTITY_MAP_BYTES - addr) {
        kprintf("range is outside the identity-mapped first 1 GiB\n");
        return;
    }

    const uint8_t *p = (const uint8_t *)(uintptr_t)addr;

    for (uint64_t off = 0; off < len; off += 16) {
        kprintf("0x%08lx ", addr + off);

        for (uint64_t i = 0; i < 16; i++) {
            if (off + i < len) {
                kprintf(" %02x", p[off + i]);
            } else {
                kprintf("   ");
            }
        }

        kprintf(" |");
        for (uint64_t i = 0; i < 16 && off + i < len; i++) {
            uint8_t b = p[off + i];
            kprintf("%c", (b >= 0x20 && b < 0x7F) ? (char)b : '.');
        }
        kprintf("|\n");
    }
}

static void cmd_selftest(int argc, char **argv) {
    (void)argc;
    (void)argv;
    selftest_run();
}

/* ---- memory commands -------------------------------------------------- */

static void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("total:     %lu KiB\n", pmm_total_memory_bytes() / 1024);
    kprintf("usable:    %lu KiB\n", pmm_usable_memory_bytes() / 1024);
    kprintf("free:      %lu frames (%lu KiB)\n", pmm_free_frame_count(),
            pmm_free_frame_count() * PMM_FRAME_SIZE / 1024);
    kprintf("used:      %lu frames\n", pmm_used_frame_count());
    kprintf("  reserved:  %lu frames\n", pmm_reserved_frame_count());
    kprintf("  allocated: %lu frames\n", pmm_allocated_frame_count());
}

static void cmd_frames(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("free_frames=%lu\n", pmm_free_frame_count());
    kprintf("used_frames=%lu\n", pmm_used_frame_count());
}

static void cmd_mmap(int argc, char **argv) {
    (void)argc;
    (void)argv;
    pmm_dump_mmap();
}

static void cmd_alloc(int argc, char **argv) {
    uint64_t count = 1;

    if (argc > 2) {
        print_usage("alloc [frames]");
        return;
    }

    if (argc == 2 && parse_arg(argv[1], "frame count", &count) != 0) {
        return;
    }

    uint64_t phys = pmm_alloc_contiguous((size_t)count);

    if (phys == 0) {
        kprintf("alloc failed (out of memory or bad count)\n");
        return;
    }

    kprintf("allocated %lu frame(s) at 0x%016lx\n", count, phys);
}

static void cmd_free(int argc, char **argv) {
    uint64_t addr;
    uint64_t count = 1;

    if (argc < 2 || argc > 3) {
        print_usage("free <addr> [frames]");
        return;
    }

    if (parse_arg(argv[1], "address", &addr) != 0) {
        return;
    }

    if (argc == 3 && parse_arg(argv[2], "frame count", &count) != 0) {
        return;
    }

    if (pmm_free_frames(addr, (size_t)count) != 0) {
        kprintf("cannot free: not an allocated, frame-aligned range\n");
        return;
    }

    kprintf("freed %lu frame(s) at 0x%016lx\n", count, addr);
}

static void cmd_kmalloc(int argc, char **argv) {
    uint64_t size;

    if (argc != 2) {
        print_usage("kmalloc <bytes>");
        return;
    }

    if (parse_arg(argv[1], "size", &size) != 0) {
        return;
    }

    void *p = kmalloc((size_t)size);

    if (!p) {
        kprintf("kmalloc failed (size 0, or no free block that large)\n");
        return;
    }

    kprintf("allocated %lu bytes at %p\n", size, p);
}

static void cmd_kfree(int argc, char **argv) {
    uint64_t addr;

    if (argc != 2) {
        print_usage("kfree <addr>");
        return;
    }

    if (parse_arg(argv[1], "address", &addr) != 0) {
        return;
    }

    /* kfree() panics on bad pointers; typed-in addresses deserve a message. */
    if (!heap_is_live_ptr((const void *)(uintptr_t)addr)) {
        kprintf("not a live heap allocation: 0x%lx\n", addr);
        return;
    }

    kfree((void *)(uintptr_t)addr);
    kprintf("freed 0x%lx\n", addr);
}

static void cmd_heap(int argc, char **argv) {
    struct heap_stats s;

    if (argc > 2 || (argc == 2 && strcmp(argv[1], "dump") != 0)) {
        print_usage("heap [dump]");
        return;
    }

    heap_get_stats(&s);

    kprintf("heap size:   %lu bytes\n", (uint64_t)s.total_bytes);
    kprintf("used:        %lu bytes in %lu block(s)\n", (uint64_t)s.used_bytes,
            (uint64_t)s.used_blocks);
    kprintf("free:        %lu bytes in %lu block(s)\n", (uint64_t)s.free_bytes,
            (uint64_t)s.free_blocks);
    kprintf("largest free: %lu bytes\n", (uint64_t)s.largest_free);
    kprintf("overhead:    %lu bytes of headers\n", (uint64_t)s.header_bytes);
    kprintf("integrity:   %s\n", heap_check() == 0 ? "ok" : "CORRUPT");

    if (argc == 2) {
        heap_dump();
    }
}

/* ---- command table ---------------------------------------------------- */

static const struct shell_command commands[] = {
    { "help",     "help",                "list commands",                    cmd_help },
    { "clear",    "clear",               "clear the screen",                 cmd_clear },
    { "about",    "about",               "show kernel name and stage",       cmd_about },
    { "echo",     "echo [text...]",      "print arguments",                  cmd_echo },
    { "date",     "date",                "wall-clock time from the RTC",     cmd_date },
    { "sleep",    "sleep <ms>",          "wait, with the CPU halted",        cmd_sleep },
    { "vtop",     "vtop <addr>",         "walk the page tables for an address", cmd_vtop },
    { "ps",       "ps",                  "list threads and scheduler stats", cmd_ps },
    { "learn",    "learn [lesson]",      "interactive OS lessons",           cmd_learn },
    { "glossary", "glossary [term]",     "look up an OS term",               cmd_glossary },
    { "ticks",    "ticks",               "timer ticks since boot",           cmd_ticks },
    { "uptime",   "uptime",              "time since boot",                  cmd_uptime },
    { "reboot",   "reboot",              "reset the machine",                cmd_reboot },
    { "halt",     "halt",                "stop the CPU",                     cmd_halt },

    { "panic",    "panic",               "trigger a kernel panic",           cmd_panic },
    { "int3",     "int3",                "trigger a breakpoint trap",        cmd_int3 },
    { "div0",     "div0",                "trigger a divide error",           cmd_div0 },
    { "pf",       "pf",                  "trigger a page fault",             cmd_pf },
    { "regs",     "regs",                "registers of the last trap",       cmd_regs },
    { "last",     "last",                "alias for regs",                   cmd_regs },
    { "hexdump",  "hexdump <addr> [len]", "dump memory (first 1 GiB only)",  cmd_hexdump },
    { "selftest", "selftest",            "run kernel self-tests",            cmd_selftest },

    { "mem",      "mem",                 "physical memory summary",          cmd_mem },
    { "frames",   "frames",              "free and used frame counts",       cmd_frames },
    { "mmap",     "mmap",                "firmware memory map",              cmd_mmap },
    { "alloc",    "alloc [frames]",      "allocate physical frame(s)",       cmd_alloc },
    { "free",     "free <addr> [frames]", "free physical frame(s)",          cmd_free },
    { "kmalloc",  "kmalloc <bytes>",     "allocate from the kernel heap",    cmd_kmalloc },
    { "kfree",    "kfree <addr>",        "free a kernel heap allocation",    cmd_kfree },
    { "heap",     "heap [dump]",         "heap statistics (and blocks)",     cmd_heap },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    kprintf("commands (numbers: decimal or 0x hex):\n");

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        kprintf("  %-22s %s\n", commands[i].usage, commands[i].help);
    }
}

/* ---- input handling --------------------------------------------------- */

void shell_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("craft> ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void shell_execute(char *line) {
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;
    char *p = line;

    /* Split on spaces in place; each argv[i] points into `line`. */
    while (*p) {
        while (*p == ' ') {
            *p++ = '\0';
        }

        if (!*p) {
            break;
        }

        if (argc == SHELL_MAX_ARGS) {
            kprintf("too many arguments (max %d)\n", SHELL_MAX_ARGS);
            return;
        }

        argv[argc++] = p;

        while (*p && *p != ' ') {
            p++;
        }
    }

    if (argc == 0) {
        return;
    }

    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].run(argc, argv);
            return;
        }
    }

    kprintf("unknown command: %s (try 'help')\n", argv[0]);
}

void shell_init(void) {
    shell_length = 0;
}

void shell_handle_char(char c) {
    if (c == '\n') {
        log_write_char('\n');
        shell_buffer[shell_length] = '\0';
        shell_execute(shell_buffer);
        shell_length = 0;
        shell_prompt();
        return;
    }

    if (c == '\b') {
        if (shell_length > 0) {
            shell_length--;
            vga_putc('\b');
            if (serial_is_ready()) {
                serial_write_char('\b');
                serial_write_char(' ');
                serial_write_char('\b');
            }
        }
        return;
    }

    /* Ignore Tab, Escape and anything else that is not printable ASCII. */
    if (c < 0x20 || c > 0x7E) {
        return;
    }

    if (shell_length < SHELL_BUFFER_SIZE - 1) {
        shell_buffer[shell_length++] = c;
        log_write_char(c);
    }
}
