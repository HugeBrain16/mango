#include "rtc.h"
#include "cmos.h"
#include "idt.h"
#include "io.h"
#include "pic.h"

volatile uint32_t rtc_ticks = 0;

static const uint8_t days_in_month[12] = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

static int is_leap_year(uint16_t year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static uint8_t month_length(uint16_t year, uint8_t month) {
    if (month == 2 && is_leap_year(year))
        return 29;
    return days_in_month[month - 1];
}

static uint8_t bcd2bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static void rtc_clear() {
    outb(CMOS_ADDR, CMOS_REG_C);
    inb(CMOS_DATA);
}

void rtc_init() {
    uint8_t rate = 6;

    cli();

    // reg A, set rate
    outb(CMOS_ADDR, 0x8A);
    uint8_t prevA = inb(CMOS_DATA);
    outb(CMOS_ADDR, 0x8A);
    outb(CMOS_DATA, (prevA & 0xF0) | rate);

    // reg B, enable periodic interrupt
    outb(CMOS_ADDR, 0x8B);
    uint8_t prevB = inb(CMOS_DATA);
    outb(CMOS_ADDR, 0x8B);
    outb(CMOS_DATA, prevB | CMOS_B_PERIODIC_INTERRUPT);

    rtc_clear();
    pic_unmask(8);

    sti();
}

void rtc_handle() {
    rtc_clear();
    rtc_ticks++;
}

void rtc_datetime(rtc_datetime_t *dt) {
    uint8_t regB;

    while (1) {
        outb(CMOS_ADDR, CMOS_REG_A);
        uint8_t status = inb(CMOS_DATA);
        if (!(status & 0x80)) break;
    }

    outb(CMOS_ADDR, CMOS_REG_B);
    regB = inb(CMOS_DATA);

    outb(CMOS_ADDR, CMOS_REG_SECONDS);
    dt->seconds = inb(CMOS_DATA);
    outb(CMOS_ADDR, CMOS_REG_MINUTES);
    dt->minutes = inb(CMOS_DATA);
    outb(CMOS_ADDR, CMOS_REG_HOURS);
    dt->hours = inb(CMOS_DATA);
    outb(CMOS_ADDR, CMOS_REG_DAY);
    dt->day = inb(CMOS_DATA);
    outb(CMOS_ADDR, CMOS_REG_MONTH);
    dt->month = inb(CMOS_DATA);
    outb(CMOS_ADDR, CMOS_REG_YEAR);
    dt->year = inb(CMOS_DATA);

    if (!(regB & CMOS_B_DATAMODE)) {
        dt->seconds = bcd2bin(dt->seconds);
        dt->minutes = bcd2bin(dt->minutes);
        dt->hours = bcd2bin(dt->hours);
        dt->day = bcd2bin(dt->day);
        dt->month = bcd2bin(dt->month);
        dt->year = bcd2bin(dt->year);
    }

    dt->year += 2000;
}

void rtc_to_local(rtc_datetime_t *dt, int tz_offset) {
    int hours = dt->hours + tz_offset;
    int day   = dt->day;
    int month = dt->month;
    int year  = dt->year;

    while (hours >= 24) {
        hours -= 24;
        day += 1;
    }

    while (hours < 0) {
        hours += 24;
        day -= 1;
    }

    while (day > month_length(year, month)) {
        day -= month_length(year, month);
        month += 1;
        if (month > 12) {
            month = 1;
            year += 1;
        }
    }

    while (day < 1) {
        month -= 1;
        if (month < 1) {
            month = 12;
            year -= 1;
        }
        day += month_length(year, month);
    }

    dt->hours = hours;
    dt->day   = day;
    dt->month = month;
    dt->year  = year;
}
