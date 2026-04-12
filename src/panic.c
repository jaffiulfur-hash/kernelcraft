#include "panic.h"
#include "arch/log.h"
#include "arch/isr.h"
#include "vga.h"

static void panic_halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void panic_banner(void) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    log_write("\n========================================\n");
    log_write("              KERNEL PANIC              \n");
    log_write("========================================\n");
}

static void panic_print_location(const char *file, int line) {
    log_write("location: ");
    if (file) {
        log_write(file);
    } else {
        log_write("(unknown)");
    }
    log_write(":");
    log_write_hex_u64((uint64_t)(unsigned int)line);
    log_write("\n");
}

static void panic_print_frame(const interrupt_frame_t *frame) {
    if (!frame) {
        log_write("frame: (none)\n");
        return;
    }

    log_write("vector: ");
    log_write_hex_u64(frame->vector);
    log_write("\nerror:  ");
    log_write_hex_u64(frame->error_code);

    log_write("\nrip:    ");
    log_write_hex_u64(frame->rip);
    log_write("\ncs:     ");
    log_write_hex_u64(frame->cs);
    log_write("\nrflags: ");
    log_write_hex_u64(frame->rflags);

    log_write("\nrax:    ");
    log_write_hex_u64(frame->rax);
    log_write("\nrbx:    ");
    log_write_hex_u64(frame->rbx);
    log_write("\nrcx:    ");
    log_write_hex_u64(frame->rcx);
    log_write("\nrdx:    ");
    log_write_hex_u64(frame->rdx);

    log_write("\nrsi:    ");
    log_write_hex_u64(frame->rsi);
    log_write("\nrdi:    ");
    log_write_hex_u64(frame->rdi);
    log_write("\nrbp:    ");
    log_write_hex_u64(frame->rbp);

    log_write("\nr8:     ");
    log_write_hex_u64(frame->r8);
    log_write("\nr9:     ");
    log_write_hex_u64(frame->r9);
    log_write("\nr10:    ");
    log_write_hex_u64(frame->r10);
    log_write("\nr11:    ");
    log_write_hex_u64(frame->r11);
    log_write("\nr12:    ");
    log_write_hex_u64(frame->r12);
    log_write("\nr13:    ");
    log_write_hex_u64(frame->r13);
    log_write("\nr14:    ");
    log_write_hex_u64(frame->r14);
    log_write("\nr15:    ");
    log_write_hex_u64(frame->r15);
    log_write("\n");
}

void panic(const char *msg) {
    panic_at(msg, 0, 0);
}

void panic_with_frame(const char *msg, const interrupt_frame_t *frame) {
    panic_with_frame_at(msg, frame, 0, 0);
}

void panic_at(const char *msg, const char *file, int line) {
    panic_banner();

    log_write("message: ");
    if (msg) {
        log_write(msg);
    } else {
        log_write("(no message)");
    }
    log_write("\n");

    if (file) {
        panic_print_location(file, line);
    }

    log_write("system halted\n");
    panic_halt_forever();
}

void panic_with_frame_at(const char *msg, const interrupt_frame_t *frame,
                         const char *file, int line) {
    panic_banner();

    log_write("message: ");
    if (msg) {
        log_write(msg);
    } else {
        log_write("(no message)");
    }
    log_write("\n");

    if (file) {
        panic_print_location(file, line);
    }

    panic_print_frame(frame);
    log_write("system halted\n");
    panic_halt_forever();
}
