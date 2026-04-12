#include "arch/log.h"
#include "arch/serial.h"
#include "vga.h"

void log_init(void) {
    serial_init();
}

void log_write(const char *s) {
    vga_write(s);
    serial_write(s);
}

void log_write_char(char c) {
    vga_putc(c);
    if (c == '\n') {
        serial_write("\n");
    } else {
        serial_write_char(c);
    }
}

void log_write_hex_u64(uint64_t value) {
    vga_write_hex_u64(value);
    serial_write_hex_u64(value);
}
