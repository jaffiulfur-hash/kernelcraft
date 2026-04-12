#ifndef PANIC_H
#define PANIC_H

#include "arch/isr.h"

void panic(const char *msg);
void panic_with_frame(const char *msg, const interrupt_frame_t *frame);

void panic_at(const char *msg, const char *file, int line);
void panic_with_frame_at(const char *msg, const interrupt_frame_t *frame,
                         const char *file, int line);

#define PANIC(msg) panic_at((msg), __FILE__, __LINE__)
#define PANIC_FRAME(msg, frame) \
    panic_with_frame_at((msg), (frame), __FILE__, __LINE__)

#endif
