#ifndef ARCH_SERIAL_H
#define ARCH_SERIAL_H

#include <stdint.h>

void serial_init(void);
int serial_is_ready(void);
void serial_write_char(char c);
void serial_write(const char *s);
void serial_write_hex_u64(uint64_t value);

#endif
