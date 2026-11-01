#include "lib/string.h"

/*
 * These loops are deliberately naive. With optimisation enabled GCC can
 * recognise them as "a memcpy" and replace the body with a call to memcpy,
 * which would recurse forever, so switch that transformation off.
 */
#define NO_LOOP_IDIOM __attribute__((optimize("no-tree-loop-distribute-patterns")))

NO_LOOP_IDIOM void *memset(void *dst, int c, size_t n) {
    uint8_t *p = (uint8_t *)dst;

    while (n--) {
        *p++ = (uint8_t)c;
    }

    return dst;
}

NO_LOOP_IDIOM void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dst;
}

NO_LOOP_IDIOM void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) {
        return dst;
    }

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        /* Overlapping with dst after src: copy backwards. */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;

    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            return (int)pa[i] - (int)pb[i];
        }
    }

    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len]) {
        len++;
    }

    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int str_to_u64(const char *s, uint64_t *out) {
    uint64_t value = 0;
    unsigned base = 10;

    if (!s || !*s) {
        return -1;
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
        if (!*s) {
            return -1;
        }
    }

    for (; *s; s++) {
        unsigned digit;
        char c = *s;

        if (c >= '0' && c <= '9') {
            digit = (unsigned)(c - '0');
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = 10u + (unsigned)(c - 'a');
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = 10u + (unsigned)(c - 'A');
        } else {
            return -1;
        }

        /* value * base + digit must still fit in 64 bits. */
        if (value > (UINT64_MAX - digit) / base) {
            return -1;
        }

        value = value * base + digit;
    }

    *out = value;
    return 0;
}
