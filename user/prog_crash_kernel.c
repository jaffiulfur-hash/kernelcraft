#include "ulib.h"

/* Reads kernel memory. The page is mapped but supervisor-only: ring 3 gets a page fault. */
int main(void) {
    uputs("about to read the kernel image at 0x100000...\n");
    volatile unsigned *kernel = (volatile unsigned *)0x100000;
    unsigned value = *kernel;

    uputs("this line should never print: ");
    uputhex(value);
    uputs("\n");
    return 0;
}
