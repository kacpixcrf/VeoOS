#ifndef VEOOS_DRIVERS_RTC_RTC_H
#define VEOOS_DRIVERS_RTC_RTC_H

#include "util/types.h"

struct rtc_date_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void rtc_read_date_time(struct rtc_date_time* result);

#endif
