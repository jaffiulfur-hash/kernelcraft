#include "arch/debug.h"
#include "arch/log.h"

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
        log_write("\nno frame\n");
        return;
    }

    log_write("\nvector=");
    log_write_hex_u64(frame->vector);

    log_write("\nerror=");
    log_write_hex_u64(frame->error_code);

    log_write("\nrip=");
    log_write_hex_u64(frame->rip);

    log_write("\ncs=");
    log_write_hex_u64(frame->cs);

    log_write("\nrflags=");
    log_write_hex_u64(frame->rflags);

    log_write("\nrax=");
    log_write_hex_u64(frame->rax);

    log_write("\nrbx=");
    log_write_hex_u64(frame->rbx);

    log_write("\nrcx=");
    log_write_hex_u64(frame->rcx);

    log_write("\nrdx=");
    log_write_hex_u64(frame->rdx);

    log_write("\nrsi=");
    log_write_hex_u64(frame->rsi);

    log_write("\nrdi=");
    log_write_hex_u64(frame->rdi);

    log_write("\nrbp=");
    log_write_hex_u64(frame->rbp);

    log_write("\nr8 =");
    log_write_hex_u64(frame->r8);

    log_write("\nr9 =");
    log_write_hex_u64(frame->r9);

    log_write("\nr10=");
    log_write_hex_u64(frame->r10);

    log_write("\nr11=");
    log_write_hex_u64(frame->r11);

    log_write("\nr12=");
    log_write_hex_u64(frame->r12);

    log_write("\nr13=");
    log_write_hex_u64(frame->r13);

    log_write("\nr14=");
    log_write_hex_u64(frame->r14);

    log_write("\nr15=");
    log_write_hex_u64(frame->r15);
    log_write("\n");
}
