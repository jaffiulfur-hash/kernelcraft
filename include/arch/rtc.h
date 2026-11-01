#ifndef ARCH_RTC_H
#define ARCH_RTC_H

#include <stdint.h>

/*
 * The battery-backed real-time clock lives in the CMOS chip and is reached
 * through two I/O ports (0x70 = register select, 0x71 = data). Unlike the
 * PIT it keeps wall-clock time while the machine is off.
 */
struct rtc_time {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
};

void rtc_read(struct rtc_time *out);
uint8_t rtc_read_seconds(void);

#endif
