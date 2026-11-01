#ifndef ARCH_QEMU_H
#define ARCH_QEMU_H

#include <stdint.h>

/*
 * QEMU's isa-debug-exit device (-device isa-debug-exit,iobase=0xf4,iosize=4)
 * turns a write to port 0xF4 into a process exit with status (code << 1) | 1.
 * That lets `make test` boot the kernel headless and read a pass/fail result.
 */
#define QEMU_EXIT_SUCCESS 0x10     /* QEMU exits with status 33 */
#define QEMU_EXIT_FAILURE 0x11     /* QEMU exits with status 35 */

/* Returns if the device is absent (e.g. a normal `make run`). */
void qemu_exit(uint8_t code);

#endif
