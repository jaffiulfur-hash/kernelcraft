#include "ulib.h"

/* Never finishes. Preemption keeps the machine usable; 'kill <pid>' ends it. */
int main(void) {
    volatile unsigned long n = 0;

    uputs("spinning forever (pid ");
    uputdec((unsigned long)ugetpid());
    uputs("): use 'kill' to stop me\n");
    for (;;) {
        n++;
    }
}
