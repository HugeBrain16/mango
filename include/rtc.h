#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include "cmos.h"

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_datetime_t;

volatile uint32_t rtc_ticks;
void rtc_init();
void rtc_handle();
void rtc_datetime(rtc_datetime_t *dt);
void rtc_to_local(rtc_datetime_t *dt, int tz_offset);

#endif
