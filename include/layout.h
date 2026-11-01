#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdint.h>

/*
 * Symbols exported by linker.ld. They have no storage of their own: only
 * their *addresses* are meaningful, so always use &__kernel_start or the
 * KERNEL_START / KERNEL_END helpers below.
 */
extern const char __kernel_start[];
extern const char __kernel_end[];

#define KERNEL_START ((uint64_t)(uintptr_t)__kernel_start)
#define KERNEL_END   ((uint64_t)(uintptr_t)__kernel_end)

/* boot.asm identity-maps the first 1 GiB with 2 MiB pages. */
#define BOOT_IDENTITY_MAP_BYTES (1ull * 1024ull * 1024ull * 1024ull)

#endif
