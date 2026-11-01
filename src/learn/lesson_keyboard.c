#include "learn/learn.h"
#include "arch/keyboard.h"
#include "lib/kprintf.h"

void lesson_keyboard(void) {
    learn_page("From keypress to character");
    learn_heading("The journey of one key");
    learn_para("1. A key closes a contact in the keyboard's matrix. The keyboard "
               "controller sends a scancode byte to the PC's 8042 chip.\n"
               "2. The 8042 raises IRQ 1. The CPU runs our handler through the "
               "IDT (see the idt and irq lessons).\n"
               "3. The handler MUST read port 0x60. Until it does, the "
               "controller sends nothing more.\n"
               "4. keyboard.c translates the scancode: it tracks Shift and Caps "
               "Lock itself, because the hardware only reports which physical "
               "key moved, not which character it means.\n"
               "5. The character goes into a 64-slot ring buffer. The handler "
               "returns immediately.\n"
               "6. The main loop was asleep in 'hlt'. The interrupt woke it, it "
               "pops the character and hands it to the shell.");
    learn_para("Step 5 is a pattern you will meet everywhere: do the minimum in "
               "the interrupt handler (the 'top half'), queue the data, and "
               "do the real work later in normal context (the 'bottom half').");
    LEARN_NEXT();

    learn_page("From keypress to character");
    learn_heading("Scancode set 1");
    learn_para("Pressing a key sends its 'make' code, e.g. 0x1E for A. "
               "Releasing sends the same code with bit 7 set: 0x9E (the "
               "'break' code). Keys like the arrows, right Ctrl and keypad "
               "Enter are 'extended' and are preceded by an 0xE0 byte, so "
               "one physical key produces two or more bytes.");
    learn_live("press keys now - raw bytes appear as they arrive; Esc to finish");

    if (learn_is_auto()) {
        kprintf("  (headless run: no keyboard input, viewer skipped)\n");
    } else {
        uint64_t seen = keyboard_raw_count();
        int done = 0;
        int after_prefix = 0;

        while (!done) {
            uint64_t total = keyboard_raw_count();

            while (seen < total && !done) {
                uint8_t sc;
                char c;

                if (!keyboard_raw_at(seen++, &sc)) {
                    continue;
                }

                if (sc == 0xE0) {
                    kprintf("  0xE0  extended-key prefix: the next byte is an extended key\n");
                    after_prefix = 1;
                    continue;
                }

                const char *kind = (sc & 0x80) ? "break (released)" : "make  (pressed) ";
                const char *ext = after_prefix ? "extended " : "";
                after_prefix = 0;

                if (sc & 0x80) {
                    kprintf("  0x%02X  %s %skey code 0x%02X\n", sc, kind, ext, sc & 0x7F);
                } else if (keyboard_try_read(&c)) {
                    if (c >= 0x20 && c < 0x7F) {
                        kprintf("  0x%02X  %s %skey  -> character '%c'\n", sc, kind, ext, c);
                    } else {
                        kprintf("  0x%02X  %s %skey  -> control character 0x%02X\n", sc, kind, ext, c);
                    }
                } else {
                    kprintf("  0x%02X  %s %skey  -> no character (Shift, arrow, ...)\n", sc, kind, ext);
                }

                if (sc == 0x01) {
                    done = 1;
                }
            }

            if (!done) {
                __asm__ volatile ("cli");
                if (keyboard_raw_count() == seen) {
                    __asm__ volatile ("sti; hlt");
                } else {
                    __asm__ volatile ("sti");
                }
            }
        }

        char junk;
        while (keyboard_try_read(&junk)) {
        }
    }

    kprintf("\n");
    learn_try("hold Shift while typing: you will see 0x2A make, then letters.");
    learn_try("press an arrow key: 0xE0 then a code, and no character at all.");
}
