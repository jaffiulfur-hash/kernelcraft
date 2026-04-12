#ifndef ARCH_DEBUG_H
#define ARCH_DEBUG_H

#include "arch/isr.h"

void debug_save_last_frame(const interrupt_frame_t *frame);
int debug_has_last_frame(void);
const interrupt_frame_t *debug_get_last_frame(void);
void debug_print_frame(const interrupt_frame_t *frame);

#endif
