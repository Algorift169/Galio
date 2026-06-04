#ifndef RTC_H
#define RTC_H

#include "common.h"
#include "kernel_time.h"

DateTime rtc_get_datetime(void);

u8 rtc_get_second(void);
u8 rtc_get_minute(void);
u8 rtc_get_hour(void);
u8 rtc_get_day(void);
u8 rtc_get_month(void);
u16 rtc_get_year(void);

void rtc_wait_for_update_complete(void);

#endif /* RTC_H */
