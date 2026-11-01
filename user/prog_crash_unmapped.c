#include "ulib.h"

/* Writes to an address nobody mapped: a page fault with "page not present". */
int main(void) {
    uputs("about to write to unmapped address 0x80000000...\n");
    *(volatile int *)0x80000000ull = 1;
    uputs("this line should never print\n");
    return 0;
}
