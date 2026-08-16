#include "rtc.h"
#include "config.h"
#include "string.h"
#include "heap.h"
#include "fio.h"

void module_time(char *dest, int offset) {
    rtc_datetime_t now;
    rtc_datetime(&now);

    char *time_config = config_get("/system/config/time.cfg", "offset");
    if (time_config) {
        offset = intstr(time_config);
        heap_free(time_config);
    }

    if (offset != 0)
        rtc_to_local(&now, offset);

    char hrs[3];
    char min[3];
    intpad(hrs, now.hours, 2, '0');
    intpad(min, now.minutes, 2, '0');

    strfmt(dest, "%s:%s", hrs, min);
}

void module_date(char *dest, int offset) {
    rtc_datetime_t now;
    rtc_datetime(&now);

    char *time_config = config_get("/system/config/time.cfg", "offset");
    if (time_config) {
        offset = intstr(time_config);
        heap_free(time_config);
    }

    if (offset != 0)
        rtc_to_local(&now, offset);

    char day[3];
    char month[3];
    intpad(day, now.day, 2, '0');
    intpad(month, now.month, 2, '0');

    strfmt(dest, "%s-%s-%d", day, month, now.year);
}