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

extern volatile uint32_t rtc_ticks;

extern void rtc_init();
extern void rtc_handle();
extern void rtc_datetime(rtc_datetime_t *dt);
extern void rtc_to_local(rtc_datetime_t *dt, int tz_offset);

extern uint64_t datetime_pack(rtc_datetime_t *dt);
extern uint64_t datetime_packed();
extern void datetime_unpack(rtc_datetime_t *dt, uint64_t time);

#endif
