#include "ulib.h"

/* Counts to five, sleeping between steps. Run two at once to watch them interleave. */
int main(void) {
    for (unsigned long i = 1; i <= 5; i++) {
        uputs("[pid ");
        uputdec((unsigned long)ugetpid());
        uputs("] count ");
        uputdec(i);
        uputs("\n");
        usleep_ms(100);
    }
    return 0;
}
