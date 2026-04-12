#include "arch/keyboard.h"
#include "arch/io.h"
#include "shell.h"

static const char scancode_set1[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8',
    '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,  '\\','z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   '7', '8', '9', '-', '4', '5', '6',
    '+', '1', '2', '3', '0', '.'
};

void keyboard_init(void) {
}

void keyboard_handle_scancode(void) {
    uint8_t scancode = inb(0x60);

    if (scancode & 0x80) {
        return;
    }

    if (scancode < sizeof(scancode_set1)) {
        char c = scancode_set1[scancode];
        if (c) {
            shell_handle_char(c);
        }
    }
}
