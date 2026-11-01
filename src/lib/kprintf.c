#include "lib/kprintf.h"
#include "arch/log.h"

struct sink {
    kformat_emit_fn emit;
    void *ctx;
    int count;
};

static void sink_put(struct sink *s, char c) {
    s->emit(c, s->ctx);
    s->count++;
}

static void sink_repeat(struct sink *s, char c, int n) {
    while (n-- > 0) {
        sink_put(s, c);
    }
}

struct spec {
    int left;      /* '-' flag */
    int zero;      /* '0' flag */
    int width;     /* minimum field width */
    int is64;      /* l / ll / z modifier */
};

/*
 * Print an unsigned magnitude in base 10 or 16.
 *
 *   negative   - print a leading '-'
 *   prefix     - literal text before the digits ("0x" for %p), or NULL
 *   min_digits - pad with leading zeros to at least this many digits
 */
static void put_number(struct sink *s, const struct spec *sp, uint64_t value,
                       unsigned base, int upper, int negative,
                       const char *prefix, int min_digits) {
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *set = upper ? upper_digits : lower_digits;

    char digits[20];            /* 2^64 - 1 has 20 decimal digits */
    int ndigits = 0;

    do {
        digits[ndigits++] = set[value % base];
        value /= base;
    } while (value != 0);

    int prefix_len = 0;
    while (prefix && prefix[prefix_len]) {
        prefix_len++;
    }

    int sign_len = negative ? 1 : 0;
    int zeros = min_digits > ndigits ? min_digits - ndigits : 0;
    int body = sign_len + prefix_len + zeros + ndigits;

    if (sp->zero && !sp->left && sp->width > body) {
        zeros += sp->width - body;
        body = sp->width;
    }

    int spaces = sp->width > body ? sp->width - body : 0;

    if (!sp->left) {
        sink_repeat(s, ' ', spaces);
    }

    if (negative) {
        sink_put(s, '-');
    }
    for (int i = 0; i < prefix_len; i++) {
        sink_put(s, prefix[i]);
    }
    sink_repeat(s, '0', zeros);
    while (ndigits > 0) {
        sink_put(s, digits[--ndigits]);
    }

    if (sp->left) {
        sink_repeat(s, ' ', spaces);
    }
}

static void put_string(struct sink *s, const struct spec *sp, const char *str) {
    int len = 0;

    if (!str) {
        str = "(null)";
    }
    while (str[len]) {
        len++;
    }

    int pad = sp->width > len ? sp->width - len : 0;

    if (!sp->left) {
        sink_repeat(s, ' ', pad);
    }
    for (int i = 0; i < len; i++) {
        sink_put(s, str[i]);
    }
    if (sp->left) {
        sink_repeat(s, ' ', pad);
    }
}

int kvformat(kformat_emit_fn emit, void *ctx, const char *fmt, va_list ap) {
    struct sink s = { emit, ctx, 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            sink_put(&s, *fmt);
            continue;
        }

        const char *spec_start = fmt;
        struct spec sp = { 0, 0, 0, 0 };

        fmt++;

        /* flags */
        for (;; fmt++) {
            if (*fmt == '-') {
                sp.left = 1;
            } else if (*fmt == '0') {
                sp.zero = 1;
            } else {
                break;
            }
        }

        /* width */
        while (*fmt >= '0' && *fmt <= '9') {
            sp.width = sp.width * 10 + (*fmt - '0');
            fmt++;
        }

        /* length */
        while (*fmt == 'l' || *fmt == 'z') {
            sp.is64 = 1;
            fmt++;
        }

        switch (*fmt) {
            case 'c':
                {
                    char ch = (char)va_arg(ap, int);
                    int pad = sp.width > 1 ? sp.width - 1 : 0;

                    if (!sp.left) {
                        sink_repeat(&s, ' ', pad);
                    }
                    sink_put(&s, ch);
                    if (sp.left) {
                        sink_repeat(&s, ' ', pad);
                    }
                }
                break;
            case 's':
                put_string(&s, &sp, va_arg(ap, const char *));
                break;
            case 'd':
            case 'i':
                {
                    int64_t v = sp.is64 ? (int64_t)va_arg(ap, long)
                                        : (int64_t)va_arg(ap, int);
                    int neg = v < 0;
                    uint64_t mag = neg ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
                    put_number(&s, &sp, mag, 10, 0, neg, 0, 0);
                }
                break;
            case 'u':
            case 'x':
            case 'X':
                {
                    uint64_t v = sp.is64 ? (uint64_t)va_arg(ap, unsigned long)
                                         : (uint64_t)va_arg(ap, unsigned int);
                    unsigned base = (*fmt == 'u') ? 10u : 16u;
                    put_number(&s, &sp, v, base, *fmt == 'X', 0, 0, 0);
                }
                break;
            case 'p':
                put_number(&s, &sp, (uint64_t)(uintptr_t)va_arg(ap, void *),
                           16, 0, 0, "0x", 16);
                break;
            case '%':
                sink_put(&s, '%');
                break;
            case '\0':
                /* Format string ended in the middle of a conversion. */
                for (; spec_start < fmt; spec_start++) {
                    sink_put(&s, *spec_start);
                }
                return s.count;
            default:
                /* Unknown conversion: print it as written. */
                for (; spec_start <= fmt; spec_start++) {
                    sink_put(&s, *spec_start);
                }
                break;
        }
    }

    return s.count;
}

static void log_emit(char c, void *ctx) {
    (void)ctx;
    log_write_char(c);
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvformat(log_emit, 0, fmt, ap);
    va_end(ap);
    return n;
}

struct buf_ctx {
    char *buf;
    size_t size;
    size_t pos;
};

static void buf_emit(char c, void *ctx) {
    struct buf_ctx *b = (struct buf_ctx *)ctx;

    if (b->size != 0 && b->pos < b->size - 1) {
        b->buf[b->pos] = c;
    }
    b->pos++;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    struct buf_ctx b = { buf, size, 0 };
    va_list ap;

    va_start(ap, fmt);
    int n = kvformat(buf_emit, &b, fmt, ap);
    va_end(ap);

    if (size != 0) {
        buf[b.pos < size - 1 ? b.pos : size - 1] = '\0';
    }

    return n;
}
