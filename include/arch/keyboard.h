#ifndef ARCH_KEYBOARD_H
#define ARCH_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);

/* Called from the IRQ1 handler: reads one scancode and queues what it means. */
void keyboard_handle_scancode(void);

/*
 * Typed characters travel through a small ring buffer: the IRQ handler is the
 * producer, ordinary kernel code (the main loop, lessons, the quiz) is the
 * consumer. The handler therefore stays tiny and everything slow runs outside
 * interrupt context with interrupts enabled.
 */
int keyboard_try_read(char *out);      /* 1 and *out set if a char was waiting */
char keyboard_getc(void);              /* waits (CPU halted) for a character */
void keyboard_push_char(char c);       /* what the IRQ handler uses; tests too */

/* Trace of raw scancode bytes, for the "keyboard" lesson. */
uint64_t keyboard_raw_count(void);                    /* bytes seen so far */
int keyboard_raw_at(uint64_t index, uint8_t *out);    /* 0 if overwritten */

#endif
