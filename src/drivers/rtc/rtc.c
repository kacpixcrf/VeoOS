#include "drivers/rtc/rtc.h"
#include "arch/x86_64/cpu/io.h"

#define CMOS_ADDRESS_PORT 0x70
#define CMOS_DATA_PORT 0x71

static uint8_t cmos_read_register(uint8_t reg) {
    io_outb(CMOS_ADDRESS_PORT, reg);
    return io_inb(CMOS_DATA_PORT);
}

static bool rtc_is_updating(void) {
    return (cmos_read_register(0x0A) & 0x80) != 0;
}

static uint8_t bcd_to_binary(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}

void rtc_read_date_time(struct rtc_date_time* result) {
    while (rtc_is_updating()) {
        __asm__ volatile("pause");
    }

    uint8_t second = cmos_read_register(0x00);
    uint8_t minute = cmos_read_register(0x02);
    uint8_t hour = cmos_read_register(0x04);
    uint8_t day = cmos_read_register(0x07);
    uint8_t month = cmos_read_register(0x08);
    uint8_t year = cmos_read_register(0x09);
    uint8_t register_b = cmos_read_register(0x0B);

    if (!(register_b & 0x04)) {
        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        hour = bcd_to_binary(hour & 0x7F) | (hour & 0x80);
        day = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year = bcd_to_binary(year);
    }

    if (!(register_b & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    result->second = second;
    result->minute = minute;
    result->hour = hour;
    result->day = day;
    result->month = month;
    result->year = 2000 + year;
}
