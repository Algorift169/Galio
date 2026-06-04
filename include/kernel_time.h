#ifndef KERNEL_TIME_H
#define KERNEL_TIME_H

#include <stdbool.h>
#include "common.h"

/* Date and time broken out into human-readable fields. */
typedef struct {
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} DateTime;

/* Initialize the kernel wall-clock from the CMOS RTC. */
void kernel_time_initialize(void);

/* Called on each PIT tick to advance software time. */
void kernel_time_update(void);

/* Read the current wall-clock as a complete DateTime struct. */
DateTime kernel_time_get_datetime(void);

/* Current wall-clock time in epoch seconds. */
u32 kernel_time_get_seconds(void);
u32 kernel_time_get_epoch_seconds(void);

/* Current uptime in seconds since boot. */
u32 kernel_time_get_uptime_seconds(void);

/* Current sub-second wall-clock precision in microseconds. */
u32 kernel_time_get_microseconds(void);

/* Backward compatibility: set wall-clock from a boot offset or config. */
void kernel_time_set_boot_seconds(u32 seconds);
void kernel_time_set_epoch_seconds(u32 seconds);
void kernel_time_set_datetime(const DateTime *dt);
void kernel_time_set_timezone_offset_seconds(s32 offset_seconds);

/* Manual synchronization helpers. */
void kernel_time_sync_with_rtc(void);
void kernel_time_sync_with_ntp(void);

/* Utility helpers for date arithmetic. */
u32 kernel_time_parse_yyyy_mm_dd_to_epoch(const char *s);
bool kernel_time_is_leap_year(int year);
int kernel_time_days_in_month(int month, int year);
void kernel_time_advance_one_second(DateTime *dt);

#endif /* KERNEL_TIME_H */
