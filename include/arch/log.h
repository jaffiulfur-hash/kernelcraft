#ifndef ARCH_LOG_H
#define ARCH_LOG_H

#include <stdint.h>

void log_init(void);
void log_write(const char *s);
void log_write_char(char c);
void log_write_hex_u64(uint64_t value);

#endif
