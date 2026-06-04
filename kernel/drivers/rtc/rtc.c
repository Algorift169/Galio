/* rtc.c - CMOS Real Time Clock driver */
#include "drivers/rtc.h"
#include "arch/x86/cpu.h"

static inline u8 rtc_read_register(u8 reg) {
    outb(0x70, (u8)(reg | 0x80));
    return inb(0x71);
}

static inline u8 rtc_bcd_to_bin(u8 val) {
    return (u8)((val & 0x0F) + ((val >> 4) * 10));
}

static bool rtc_is_valid_datetime(const DateTime *dt) {
    if (!dt) return false;
    if (dt->year < 1970) return false;
    if (dt->month < 1 || dt->month > 12) return false;
    if (dt->day < 1 || dt->day > 31) return false;
    if (dt->hour > 23) return false;
    if (dt->minute > 59) return false;
    if (dt->second > 59) return false;
    return true;
}

static DateTime rtc_read_datetime_once(void) {
    DateTime dt = {0};
    u8 sec = rtc_read_register(0x00);
    u8 min = rtc_read_register(0x02);
    u8 hour = rtc_read_register(0x04);
    u8 day = rtc_read_register(0x07);
    u8 month = rtc_read_register(0x08);
    u8 year = rtc_read_register(0x09);
    u8 century = rtc_read_register(0x32);
    u8 regb = rtc_read_register(0x0B);

    bool is_bcd = !(regb & 0x04);
    bool is_24h = (regb & 0x02);
    bool is_pm = (hour & 0x80) != 0;
    hour &= 0x7F;

    if (is_bcd) {
        sec = rtc_bcd_to_bin(sec);
        min = rtc_bcd_to_bin(min);
        hour = rtc_bcd_to_bin(hour);
        day = rtc_bcd_to_bin(day);
        month = rtc_bcd_to_bin(month);
        year = rtc_bcd_to_bin(year);
        if (century) century = rtc_bcd_to_bin(century);
    }

    if (!is_24h) {
        if (is_pm) {
            if (hour != 12) {
                hour = (hour + 12) % 24;
            }
        } else if (hour == 12) {
            hour = 0;
        }
    }

    dt.second = sec;
    dt.minute = min;
    dt.hour = hour;
    dt.day = day;
    dt.month = month;

    if (century) {
        dt.year = (u16)century * 100 + (u16)year;
    } else {
        dt.year = (year >= 70) ? (u16)(1900 + year) : (u16)(2000 + year);
    }

    if (!rtc_is_valid_datetime(&dt)) {
        dt.year = 0;
        dt.month = 0;
        dt.day = 0;
        dt.hour = 0;
        dt.minute = 0;
        dt.second = 0;
    }

    return dt;
}

void rtc_wait_for_update_complete(void) {
    while (rtc_read_register(0x0A) & 0x80);
}

DateTime rtc_get_datetime(void) {
    DateTime first;
    DateTime second;

    do {
        rtc_wait_for_update_complete();
        first = rtc_read_datetime_once();
        rtc_wait_for_update_complete();
        second = rtc_read_datetime_once();
    } while (first.second != second.second ||
             first.minute != second.minute ||
             first.hour != second.hour ||
             first.day != second.day ||
             first.month != second.month ||
             first.year != second.year);

    return first;
}

u8 rtc_get_second(void) {
    return rtc_get_datetime().second;
}

u8 rtc_get_minute(void) {
    return rtc_get_datetime().minute;
}

u8 rtc_get_hour(void) {
    return rtc_get_datetime().hour;
}

u8 rtc_get_day(void) {
    return rtc_get_datetime().day;
}

u8 rtc_get_month(void) {
    return rtc_get_datetime().month;
}

u16 rtc_get_year(void) {
    return rtc_get_datetime().year;
}
