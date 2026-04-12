#include "shell.h"
#include "panic.h"
#include "vga.h"
#include "arch/debug.h"
#include "arch/log.h"
#include "arch/pit.h"
#include "arch/serial.h"
#include "mm/pmm.h"

#define SHELL_BUFFER_SIZE 128
#define PIT_HZ 100

static char shell_buffer[SHELL_BUFFER_SIZE];
static int shell_length = 0;

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
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

void shell_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    log_write("\ncraft> ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void print_uptime(void) {
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / PIT_HZ;

    log_write("\nuptime ticks=");
    log_write_hex_u64(ticks);
    log_write(" seconds=");
    log_write_hex_u64(secs);
}

static void shell_execute(const char *cmd) {
    if (str_equal(cmd, "help")) {
        log_write("\ncommands: help clear about ticks uptime panic int3 div0 last regs mem frames alloc free mmap");
    } else if (str_equal(cmd, "clear")) {
        vga_clear();
    } else if (str_equal(cmd, "about")) {
        log_write("\nKernelCraft OS - educational kernel");
        log_write("\nstage 3.3 - serial logging and debug traps");
    } else if (str_equal(cmd, "ticks")) {
        log_write("\nticks=");
        log_write_hex_u64(pit_get_ticks());
    } else if (str_equal(cmd, "uptime")) {
        print_uptime();
    } else if (str_equal(cmd, "panic")) {
        panic("panic command invoked");
    } else if (str_equal(cmd, "int3")) {
        log_write("\ntriggering breakpoint...");
        trigger_breakpoint();
        log_write(" returned from breakpoint");
    } else if (str_equal(cmd, "div0")) {
        log_write("\ntriggering divide by zero...");
        trigger_divide_by_zero();
    } else if (str_equal(cmd, "last") || str_equal(cmd, "regs")) {
        if (debug_has_last_frame()) {
            debug_print_frame(debug_get_last_frame());
        } else {
            log_write("\nno saved interrupt frame");
        }
    } else if (str_equal(cmd, "mem")) {
        log_write("\ntotal=");
        log_write_hex_u64(pmm_total_memory_bytes());
        log_write("\nusable=");
        log_write_hex_u64(pmm_usable_memory_bytes());
        log_write("\nfree_frames=");
        log_write_hex_u64(pmm_free_frame_count());
        log_write("\nused_frames=");
        log_write_hex_u64(pmm_used_frame_count());
    } else if (str_equal(cmd, "frames")) {
        log_write("\nfree_frames=");
        log_write_hex_u64(pmm_free_frame_count());
        log_write("\nused_frames=");
        log_write_hex_u64(pmm_used_frame_count());
    } else if (str_equal(cmd, "alloc")) {
        uint64_t phys = pmm_alloc_frame();
        log_write("\nalloc=");
        log_write_hex_u64(phys);
    } else if (str_equal(cmd, "free")) {
        log_write("\nfree command not yet parameterized");
    } else if (str_equal(cmd, "mmap")) {
        pmm_dump_mmap();
    } else if (cmd[0] != '\0') {
        log_write("\nunknown command: ");
        log_write(cmd);
    }
}

void shell_init(void) {
    shell_length = 0;
}

void shell_handle_char(char c) {
    if (c == '\n') {
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

    if (shell_length < SHELL_BUFFER_SIZE - 1) {
        shell_buffer[shell_length++] = c;
        log_write_char(c);
    }
}
