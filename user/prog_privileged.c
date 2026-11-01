#include "ulib.h"

/* cli is a privileged instruction: in ring 3 it raises a general protection fault. */
int main(void) {
    uputs("about to execute the privileged instruction 'cli'...\n");
    __asm__ volatile ("cli");
    uputs("this line should never print\n");
    return 0;
}
