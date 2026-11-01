#include "ulib.h"

int main(void) {
    uputs("Hello from user mode! pid=");
    uputdec((unsigned long)ugetpid());
    uputs(", CS=0x");
    uputhex(ucs());
    uputs(" so my privilege level is ring ");
    uputdec(ucs() & 3);
    uputs("\n");
    return 0;
}
