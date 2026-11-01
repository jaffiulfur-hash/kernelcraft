#include "arch/debug.h"
#include "lib/kprintf.h"

static interrupt_frame_t last_frame;
static int have_last_frame = 0;

void debug_save_last_frame(const interrupt_frame_t *frame) {
    last_frame = *frame;
    have_last_frame = 1;
}

int debug_has_last_frame(void) {
    return have_last_frame;
}

const interrupt_frame_t *debug_get_last_frame(void) {
    return have_last_frame ? &last_frame : 0;
}

void debug_print_frame(const interrupt_frame_t *frame) {
    if (!frame) {
        kprintf("no frame\n");
        return;
    }

    kprintf("vector=0x%016lx\n", frame->vector);
    kprintf("error=0x%016lx\n", frame->error_code);
    kprintf("rip=0x%016lx\n", frame->rip);
    kprintf("cs=0x%016lx\n", frame->cs);
    kprintf("rflags=0x%016lx\n", frame->rflags);
    kprintf("rax=0x%016lx\n", frame->rax);
    kprintf("rbx=0x%016lx\n", frame->rbx);
    kprintf("rcx=0x%016lx\n", frame->rcx);
    kprintf("rdx=0x%016lx\n", frame->rdx);
    kprintf("rsi=0x%016lx\n", frame->rsi);
    kprintf("rdi=0x%016lx\n", frame->rdi);
    kprintf("rbp=0x%016lx\n", frame->rbp);
    kprintf("r8 =0x%016lx\n", frame->r8);
    kprintf("r9 =0x%016lx\n", frame->r9);
    kprintf("r10=0x%016lx\n", frame->r10);
    kprintf("r11=0x%016lx\n", frame->r11);
    kprintf("r12=0x%016lx\n", frame->r12);
    kprintf("r13=0x%016lx\n", frame->r13);
    kprintf("r14=0x%016lx\n", frame->r14);
    kprintf("r15=0x%016lx\n", frame->r15);
}
