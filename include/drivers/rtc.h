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
