#ifndef ARCH_KEYBOARD_H
#define ARCH_KEYBOARD_H

void keyboard_init(void);

/* Called from the IRQ1 handler: reads one scancode and feeds the shell. */
void keyboard_handle_scancode(void);

#endif
