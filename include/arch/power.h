#ifndef ARCH_POWER_H
#define ARCH_POWER_H

/* Stop the CPU for good (interrupts disabled). */
void arch_halt(void) __attribute__((noreturn));

/* Reset the machine via the keyboard controller, then a triple fault. */
void arch_reboot(void) __attribute__((noreturn));

#endif
