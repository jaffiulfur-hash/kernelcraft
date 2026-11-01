#include "arch/rtc.h"
#include "arch/io.h"

#define CMOS_INDEX  0x70
#define CMOS_DATA   0x71

#define REG_SECONDS 0x00
#define REG_MINUTES 0x02
#define REG_HOURS   0x04
#define REG_DAY     0x07
#define REG_MONTH   0x08
#define REG_YEAR    0x09
#define REG_STATUS_A 0x0A
#define REG_STATUS_B 0x0B

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_INDEX, reg);      /* bit 7 stays 0: leave NMI enabled */
    return inb(CMOS_DATA);
}

static int update_in_progress(void) {
    return cmos_read(REG_STATUS_A) & 0x80;
}

static uint8_t from_bcd(uint8_t v) {
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

struct raw_time {
    uint8_t sec, min, hour, day, month, year;
};

static void read_raw(struct raw_time *r) {
    while (update_in_progress()) {
    }
    r->sec = cmos_read(REG_SECONDS);
    r->min = cmos_read(REG_MINUTES);
    r->hour = cmos_read(REG_HOURS);
    r->day = cmos_read(REG_DAY);
    r->month = cmos_read(REG_MONTH);
    r->year = cmos_read(REG_YEAR);
}

uint8_t rtc_read_seconds(void) {
    struct rtc_time t;

    rtc_read(&t);
    return t.second;
}

void rtc_read(struct rtc_time *out) {
    struct raw_time a, b;

    /* The clock can tick while we read: read until two passes agree. */
    read_raw(&a);
    do {
        b = a;
        read_raw(&a);
    } while (a.sec != b.sec || a.min != b.min || a.hour != b.hour ||
             a.day != b.day || a.month != b.month || a.year != b.year);

    uint8_t status_b = cmos_read(REG_STATUS_B);
    int pm = a.hour & 0x80;

    a.hour &= 0x7F;
    if (!(status_b & 0x04)) {           /* values are BCD, not binary */
        a.sec = from_bcd(a.sec);
        a.min = from_bcd(a.min);
        a.hour = from_bcd(a.hour);
        a.day = from_bcd(a.day);
        a.month = from_bcd(a.month);
        a.year = from_bcd(a.year);
    }
    if (!(status_b & 0x02) && pm) {     /* 12-hour clock, PM */
        a.hour = (uint8_t)((a.hour % 12) + 12);
    }

    out->second = a.sec;
    out->minute = a.min;
    out->hour = a.hour;
    out->day = a.day;
    out->month = a.month;
    out->year = (uint16_t)(2000 + a.year);   /* century register ignored */
}
