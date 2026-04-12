#include "arch/serial.h"
#include "arch/io.h"

#define COM1 0x3F8

static int serial_ready = 0;

static int serial_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);

    serial_ready = 1;
}

int serial_is_ready(void) {
    return serial_ready;
}

void serial_write_char(char c) {
    if (!serial_ready) {
        return;
    }

    while (!serial_transmit_empty()) {
    }

    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    if (!serial_ready) {
        return;
    }

    while (*s) {
        if (*s == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*s++);
    }
}

void serial_write_hex_u64(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";

    serial_write("0x");
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0xF);
        serial_write_char(hex[nibble]);
    }
}
