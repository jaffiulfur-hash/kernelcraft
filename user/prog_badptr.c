#include "ulib.h"

/*
 * Hands the kernel hostile pointers. The kernel must refuse each one (return
 * -1) rather than read kernel memory on our behalf or crash. Exit code 0 means
 * every check came out the way it should.
 */
int main(void) {
    char local[8] = "stack!\n";
    int bad = 0;

    long a = usyscall(SYS_WRITE, 0x100000, 16, 0);        /* kernel memory */
    long b = usyscall(SYS_WRITE, 0x80000000L, 16, 0);     /* nothing mapped there */
    long c = usyscall(SYS_WRITE, 0x401FFFF8L, 64, 0);     /* starts valid, runs past the window */
    long d = usyscall(SYS_WRITE, (long)local, 7, 0);      /* my own stack: perfectly fine */

    uputs("kernel pointer      -> "); uputdec((unsigned long)(a == -1)); uputs(" (1 = refused)\n");
    uputs("unmapped pointer    -> "); uputdec((unsigned long)(b == -1)); uputs(" (1 = refused)\n");
    uputs("range past the end  -> "); uputdec((unsigned long)(c == -1)); uputs(" (1 = refused)\n");
    uputs("own stack, 7 bytes  -> wrote "); uputdec((unsigned long)d); uputs("\n");

    if (a != -1 || b != -1 || c != -1 || d != 7) {
        bad = 1;
    }
    return bad;
}
