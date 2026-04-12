#ifndef ARCH_IDT_H
#define ARCH_IDT_H

#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t vector, void *handler, uint8_t flags);

#endif
