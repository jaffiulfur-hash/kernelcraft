#include "ulib.h"

void uputs(const char *s) {
    size_t n = 0;

    while (s[n]) {
        n++;
    }
    usyscall(SYS_WRITE, (long)s, (long)n, 0);
}

void uputdec(unsigned long v) {
    char buf[24];
    int i = 23;

    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    uputs(&buf[i]);
}

void uputhex(unsigned long v) {
    static const char digits[] = "0123456789abcdef";
    char buf[17];

    for (int i = 15; i >= 0; i--) {
        buf[i] = digits[v & 0xF];
        v >>= 4;
    }
    buf[16] = '\0';
    uputs(buf);
}
