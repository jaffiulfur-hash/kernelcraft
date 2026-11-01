#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stddef.h>
#include <stdint.h>

/*
 * Minimal freestanding string/memory helpers.
 *
 * GCC may emit calls to memcpy/memset/memmove/memcmp on its own (struct
 * copies, zero-initialisation), even with -ffreestanding, so the kernel has
 * to provide them.
 */
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

/*
 * Parse an unsigned number: decimal ("123") or hexadecimal ("0x7B").
 * Returns 0 and stores the value in *out on success, -1 on empty input,
 * stray characters or overflow (*out is left untouched on failure).
 */
int str_to_u64(const char *s, uint64_t *out);

#endif
