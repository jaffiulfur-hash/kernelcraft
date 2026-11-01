#include "boot/bootinfo.h"

static uint32_t saved_magic;
static uint32_t saved_addr;

void bootinfo_init(uint32_t magic, uint32_t info_addr) {
    saved_magic = magic;
    saved_addr = info_addr;
}

uint32_t bootinfo_magic(void) {
    return saved_magic;
}

uint32_t bootinfo_addr(void) {
    return saved_addr;
}
