#include "arch/interrupts.h"
#include "arch/cpu.h"
#include "arch/debug.h"
#include "arch/idt.h"
#include "arch/isr.h"
#include "arch/keyboard.h"
#include "arch/log.h"
#include "arch/pic.h"
#include "arch/pit.h"
#include "lib/kprintf.h"
#include "vga.h"

static const char *exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

static uint64_t vector_counts[256];

uint64_t interrupt_count(uint8_t vector) {
    return vector_counts[vector];
}

const char *interrupt_vector_name(uint8_t vector) {
    static const char *irq_names[16] = {
        "IRQ 0 (timer)", "IRQ 1 (keyboard)", "IRQ 2 (cascade)", "IRQ 3 (COM2)",
        "IRQ 4 (COM1)", "IRQ 5", "IRQ 6 (floppy)", "IRQ 7 (LPT1)",
        "IRQ 8 (RTC)", "IRQ 9", "IRQ 10", "IRQ 11", "IRQ 12 (mouse)",
        "IRQ 13 (FPU)", "IRQ 14 (ATA 1)", "IRQ 15 (ATA 2)"
    };

    if (vector < 32) {
        return exception_names[vector];
    }
    if (vector < 48) {
        return irq_names[vector - 32];
    }
    if (vector == 0x80) {
        return "System call";
    }
    return "(unused)";
}

/* Decode the page-fault error code and the faulting address in CR2. */
static void print_page_fault_details(uint64_t error) {
    kprintf("page fault at address 0x%016lx\n", read_cr2());
    kprintf("  cause: %s while %s in %s mode%s\n",
            (error & 1) ? "protection violation" : "page not present",
            (error & 2) ? "writing" : "reading",
            (error & 4) ? "user" : "supervisor",
            (error & 16) ? " (instruction fetch)" : "");
}

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void print_exception_name(uint64_t vector) {
    if (vector < 32) {
        log_write(exception_names[vector]);
    } else {
        log_write("Unknown");
    }
}

void interrupt_dispatch(interrupt_frame_t *frame) {
    vector_counts[frame->vector & 0xFF]++;

    if (frame->vector < 32) {
        debug_save_last_frame(frame);

        if (frame->vector == 3) {
            vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
            log_write("\nDEBUG TRAP: ");
            print_exception_name(frame->vector);
            log_write("\n");
            debug_print_frame(frame);
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            return;
        }

        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        log_write("\nEXCEPTION: ");
        print_exception_name(frame->vector);
        log_write("\n");
        if (frame->vector == 14) {
            print_page_fault_details(frame->error_code);
        }
        debug_print_frame(frame);
        halt_forever();
    }

    if (frame->vector >= 32 && frame->vector <= 47) {
        uint8_t irq = (uint8_t)(frame->vector - 32);

        switch (irq) {
            case 0:
                pit_tick();
                break;
            case 1:
                keyboard_handle_scancode();
                break;
            default:
                break;
        }

        pic_send_eoi(irq);
        return;
    }

    debug_save_last_frame(frame);

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    log_write("\nUnhandled interrupt vector=");
    log_write_hex_u64(frame->vector);
    log_write("\n");
    debug_print_frame(frame);
    halt_forever();
}

void interrupts_init(void) {
    idt_init();

    idt_set_gate(32, irq0,  0x8E);
    idt_set_gate(33, irq1,  0x8E);
    idt_set_gate(34, irq2,  0x8E);
    idt_set_gate(35, irq3,  0x8E);
    idt_set_gate(36, irq4,  0x8E);
    idt_set_gate(37, irq5,  0x8E);
    idt_set_gate(38, irq6,  0x8E);
    idt_set_gate(39, irq7,  0x8E);
    idt_set_gate(40, irq8,  0x8E);
    idt_set_gate(41, irq9,  0x8E);
    idt_set_gate(42, irq10, 0x8E);
    idt_set_gate(43, irq11, 0x8E);
    idt_set_gate(44, irq12, 0x8E);
    idt_set_gate(45, irq13, 0x8E);
    idt_set_gate(46, irq14, 0x8E);
    idt_set_gate(47, irq15, 0x8E);

    pic_remap(32, 40);
    pic_mask_all();
    pic_unmask_irq(0);
    pic_unmask_irq(1);

    __asm__ volatile ("sti");
}
