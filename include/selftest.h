#ifndef SELFTEST_H
#define SELFTEST_H

#include <stdint.h>

/* Remember the Multiboot2 info address so tests can check its reservation. */
void selftest_init(uint32_t mb_info_addr);

/* Run every kernel self-test. Returns the number of tests that failed. */
int selftest_run(void);

#endif
