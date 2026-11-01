#include "ulib.h"

/* Lives at the same virtual address in every process that runs this program. */
static volatile unsigned long secret;

int main(void) {
    unsigned long pid = (unsigned long)ugetpid();
    unsigned long mine = pid * 1000 + 7;

    secret = mine;
    uputs("[pid ");
    uputdec(pid);
    uputs("] my secret lives at virtual 0x");
    uputhex((unsigned long)&secret);
    uputs(" and holds ");
    uputdec(secret);
    uputs("\n");

    usleep_ms(400);                     /* every other copy writes ITS value to this address */

    if (secret != mine) {
        uputs("[pid ");
        uputdec(pid);
        uputs("] my secret was overwritten by another process!\n");
        return 1;
    }
    uputs("[pid ");
    uputdec(pid);
    uputs("] my secret is intact: separate address space\n");
    return 0;
}
