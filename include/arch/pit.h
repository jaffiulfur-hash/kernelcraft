#ifndef ARCH_PIT_H
#define ARCH_PIT_H

#include <stdint.h>

/* Tick rate the kernel programs the PIT to. */
#define PIT_HZ 100

void pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);
void pit_tick(void);

#endif
