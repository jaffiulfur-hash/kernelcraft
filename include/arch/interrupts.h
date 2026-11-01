#ifndef ARCH_INTERRUPTS_H
#define ARCH_INTERRUPTS_H

#include <stdint.h>

void interrupts_init(void);

/* How many times a vector has fired since boot. */
uint64_t interrupt_count(uint8_t vector);

/* "Divide Error", "IRQ 1", "System call", ... for display. */
const char *interrupt_vector_name(uint8_t vector);

#endif
