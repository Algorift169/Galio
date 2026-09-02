/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

#include "kernel_time.h"
#include "drivers/pit.h"
#include "drivers/rtc.h"
#include "kprintf.h"
#include "lib/string.h"
#include "arch/x86/cpu.h"

static DateTime current_datetime = {0};
static u32 base_epoch_seconds = 0;
static u32 wall_epoch_seconds = 0;
static u32 uptime_ticks = 0;
static s32 timezone_offset_seconds = 6 * 3600; /* default to Bangladesh UTC+6 */
static bool time_initialized = false;

static bool kernel_time_is_valid_datetime(const DateTime *dt) {
    if (!dt) return false;
    if (dt->year < 1970) return false;
    if (dt->month < 1 || dt->month > 12) return false;
    if (dt->day < 1 || dt->day > 31) return false;
    if (dt->hour > 23) return false;
    if (dt->minute > 59) return false;
    if (dt->second > 59) return false;
    return true;
}

bool kernel_time_is_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

int kernel_time_days_in_month(int month, int year) {
    static const int days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    if (month == 2 && kernel_time_is_leap_year(year)) return 29;
    return days_per_month[month - 1];
}

void kernel_time_advance_one_second(DateTime *dt) {
    if (!dt) return;

    dt->second++;
    if (dt->second >= 60) {
        dt->second = 0;
        dt->minute++;
        if (dt->minute >= 60) {
            dt->minute = 0;
            dt->hour++;
            if (dt->hour >= 24) {
                dt->hour = 0;
                dt->day++;
                if (dt->day > (u8)kernel_time_days_in_month(dt->month, dt->year)) {
                    dt->day = 1;
                    dt->month++;
                    if (dt->month > 12) {
                        dt->month = 1;
                        dt->year++;
                    }
                }
            }
        }
    }
}

static u32 kernel_time_datetime_to_epoch(const DateTime *dt) {
    if (!dt || !kernel_time_is_valid_datetime(dt)) return 0;

    u32 days = 0;
    for (int year = 1970; year < (int)dt->year; year++) {
        days += 365 + (kernel_time_is_leap_year(year) ? 1 : 0);
    }
    for (int month = 1; month < dt->month; month++) {
        days += kernel_time_days_in_month(month, dt->year);
    }
    days += (u32)(dt->day - 1);
    return days * 86400u + (u32)dt->hour * 3600u + (u32)dt->minute * 60u + (u32)dt->second;
}

static DateTime kernel_time_datetime_from_epoch(u32 epoch) {
    DateTime dt = {0};
    dt.second = epoch % 60u;
    dt.minute = (epoch / 60u) % 60u;
    dt.hour = (epoch / 3600u) % 24u;

    u32 days = epoch / 86400u;
    int year = 1970;
    while (true) {
        int year_days = 365 + (kernel_time_is_leap_year(year) ? 1 : 0);
        if (days >= (u32)year_days) {
            days -= (u32)year_days;
            year++;
        } else {
            break;
        }
    }
    dt.year = (u16)year;

    int month = 1;
    while (true) {
        int dim = kernel_time_days_in_month(month, year);
        if (days >= (u32)dim) {
            days -= (u32)dim;
            month++;
        } else {
            break;
        }
    }
    dt.month = (u8)month;
    dt.day = (u8)(days + 1);
    return dt;
}

static void kernel_time_apply_offset(void) {
    s64 adjusted = (s64)base_epoch_seconds + (s64)timezone_offset_seconds;
    if (adjusted < 0) {
        adjusted = 0;
    }
    wall_epoch_seconds = (u32)adjusted;
    current_datetime = kernel_time_datetime_from_epoch(wall_epoch_seconds);
}

void kernel_time_update(void) {
    uptime_ticks++;
    if (uptime_ticks % 100u == 0u) {
        base_epoch_seconds++;
        s64 adjusted = (s64)base_epoch_seconds + (s64)timezone_offset_seconds;
        if (adjusted < 0) {
            adjusted = 0;
        }
        wall_epoch_seconds = (u32)adjusted;
        current_datetime = kernel_time_datetime_from_epoch(wall_epoch_seconds);
    }
}

static void kernel_time_tick(registers_t *regs) {
    (void)regs;
    kernel_time_update();
}

void kernel_time_initialize(void) {
    DateTime rtc_time = rtc_get_datetime();
    if (!kernel_time_is_valid_datetime(&rtc_time)) {
        kprintf("[TIME] RTC read failed or returned invalid values\n");
        current_datetime = (DateTime){0};
        base_epoch_seconds = 0;
        wall_epoch_seconds = 0;
    } else {
        current_datetime = rtc_time;
        base_epoch_seconds = kernel_time_datetime_to_epoch(&rtc_time);
        wall_epoch_seconds = base_epoch_seconds;
        kprintf("[TIME] RTC raw: %04u-%02u-%02u %02u:%02u:%02u\n",
                rtc_time.year,
                rtc_time.month,
                rtc_time.day,
                rtc_time.hour,
                rtc_time.minute,
                rtc_time.second);
        kprintf("[TIME] RTC epoch: %u\n", base_epoch_seconds);
        kprintf("[TIME] Timezone offset before apply: %d seconds\n", timezone_offset_seconds);
        kernel_time_apply_offset();
        kprintf("[TIME] Adjusted epoch: %u\n", wall_epoch_seconds);
        DateTime adjusted = kernel_time_get_datetime();
        kprintf("[TIME] Final wall-clock: %04u-%02u-%02u %02u:%02u:%02u\n",
                adjusted.year,
                adjusted.month,
                adjusted.day,
                adjusted.hour,
                adjusted.minute,
                adjusted.second);
        if (timezone_offset_seconds != 0) {
            kprintf("[TIME] Timezone offset applied: %d seconds\n",
                    timezone_offset_seconds);
        }
    }
    uptime_ticks = 0;
    time_initialized = true;
    pit_install_callback(kernel_time_tick);
}

DateTime kernel_time_get_datetime(void) {
    return current_datetime;
}

u32 kernel_time_get_seconds(void) {
    return wall_epoch_seconds;
}

u32 kernel_time_get_epoch_seconds(void) {
    return wall_epoch_seconds;
}

u32 kernel_time_get_uptime_seconds(void) {
    return uptime_ticks / 100u;
}

u32 kernel_time_get_microseconds(void) {
    return (uptime_ticks % 100u) * 10000u;
}

void kernel_time_set_boot_seconds(u32 seconds) {
    kernel_time_set_epoch_seconds(seconds);
}

void kernel_time_set_epoch_seconds(u32 seconds) {
    base_epoch_seconds = seconds;
    s64 adjusted = (s64)base_epoch_seconds + (s64)timezone_offset_seconds;
    if (adjusted < 0) {
        adjusted = 0;
    }
    wall_epoch_seconds = (u32)adjusted;
    current_datetime = kernel_time_datetime_from_epoch(wall_epoch_seconds);
}

void kernel_time_set_datetime(const DateTime *dt) {
    if (!dt || !kernel_time_is_valid_datetime(dt)) return;
    base_epoch_seconds = kernel_time_datetime_to_epoch(dt);
    s64 adjusted = (s64)base_epoch_seconds + (s64)timezone_offset_seconds;
    if (adjusted < 0) {
        adjusted = 0;
    }
    wall_epoch_seconds = (u32)adjusted;
    current_datetime = kernel_time_datetime_from_epoch(wall_epoch_seconds);
}

void kernel_time_set_timezone_offset_seconds(s32 offset_seconds) {
    timezone_offset_seconds = offset_seconds;
    kprintf("[TIME] timezone_offset_seconds set: %d\n", timezone_offset_seconds);
    if (time_initialized) {
        kernel_time_apply_offset();
        kprintf("[TIME] timezone offset applied in setter; final epoch=%u\n", wall_epoch_seconds);
        DateTime adjusted = kernel_time_get_datetime();
        kprintf("[TIME] final displayed time after offset: %04u-%02u-%02u %02u:%02u:%02u\n",
                adjusted.year,
                adjusted.month,
                adjusted.day,
                adjusted.hour,
                adjusted.minute,
                adjusted.second);
    }
}

void kernel_time_sync_with_rtc(void) {
    DateTime rtc_time = rtc_get_datetime();
    if (!kernel_time_is_valid_datetime(&rtc_time)) {
        kprintf("[TIME] RTC synchronization failed: invalid RTC values\n");
        return;
    }
    base_epoch_seconds = kernel_time_datetime_to_epoch(&rtc_time);
    s64 adjusted = (s64)base_epoch_seconds + (s64)timezone_offset_seconds;
    if (adjusted < 0) {
        adjusted = 0;
    }
    wall_epoch_seconds = (u32)adjusted;
    current_datetime = kernel_time_datetime_from_epoch(wall_epoch_seconds);
    kprintf("[TIME] Synchronized clock from RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
            rtc_time.year,
            rtc_time.month,
            rtc_time.day,
            rtc_time.hour,
            rtc_time.minute,
            rtc_time.second);
}

void kernel_time_sync_with_ntp(void) {
    kprintf("[TIME] NTP sync not implemented yet\n");
}

u32 kernel_time_parse_yyyy_mm_dd_to_epoch(const char *s) {
    if (!s) return 0;

    int y = 0;
    int m = 0;
    int d = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const char *p = s;

    for (int i = 0; i < 4; i++) {
        if (*p < '0' || *p > '9') return 0;
        y = y * 10 + (*p++ - '0');
    }
    if (*p != '-') return 0;
    p++;
    for (int i = 0; i < 2; i++) {
        if (*p < '0' || *p > '9') return 0;
        m = m * 10 + (*p++ - '0');
    }
    if (*p != '-') return 0;
    p++;
    for (int i = 0; i < 2; i++) {
        if (*p < '0' || *p > '9') return 0;
        d = d * 10 + (*p++ - '0');
    }

    if (*p == ' ' || *p == 'T') {
        p++;
        for (int i = 0; i < 2; i++) {
            if (*p < '0' || *p > '9') return 0;
            hour = hour * 10 + (*p++ - '0');
        }
        if (*p != ':') return 0;
        p++;
        for (int i = 0; i < 2; i++) {
            if (*p < '0' || *p > '9') return 0;
            minute = minute * 10 + (*p++ - '0');
        }
        if (*p != ':') return 0;
        p++;
        for (int i = 0; i < 2; i++) {
            if (*p < '0' || *p > '9') return 0;
            second = second * 10 + (*p++ - '0');
        }
    }

    while (*p == ' ') p++;
    if (*p != '\0' && *p != '\n' && *p != '\r') return 0;
    if (m < 1 || m > 12) return 0;
    if (d < 1 || d > kernel_time_days_in_month(m, y)) return 0;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return 0;

    u32 days = 0;
    for (int year = 1970; year < y; year++) {
        days += 365 + (kernel_time_is_leap_year(year) ? 1 : 0);
    }
    for (int month = 1; month < m; month++) {
        days += kernel_time_days_in_month(month, y);
    }
    days += (u32)(d - 1);
    return days * 86400u + (u32)hour * 3600u + (u32)minute * 60u + (u32)second;
}
