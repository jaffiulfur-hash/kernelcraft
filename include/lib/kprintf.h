#ifndef LIB_KPRINTF_H
#define LIB_KPRINTF_H

#include <stdarg.h>
#include <stddef.h>

/*
 * A small printf for the kernel.
 *
 * Conversions: %c %s %d %i %u %x %X %p %%
 * Flags:       '-' (left-justify), '0' (zero-pad)
 * Width:       decimal digits, e.g. %08x or %-12s
 * Length:      l, ll, z (all 64-bit on x86_64); none means 32-bit
 *
 * %p always prints 0x followed by 16 hex digits, matching log_write_hex_u64.
 * A NULL %s prints "(null)". Unknown conversions are printed verbatim.
 */

/* Callback that receives the formatted output one character at a time. */
typedef void (*kformat_emit_fn)(char c, void *ctx);

/* Format into an arbitrary sink. Returns the number of characters produced. */
int kvformat(kformat_emit_fn emit, void *ctx, const char *fmt, va_list ap);

/* Print to the kernel log (VGA + serial). */
int kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/*
 * Format into a buffer, always NUL-terminating when size > 0. Like snprintf,
 * returns the length the full output would have had, so a result >= size
 * means the output was truncated.
 */
int ksnprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#endif
