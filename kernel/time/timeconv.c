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

/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * timeconv.c - Time conversion utilities for the Galio kernel.
 *
 * Linux analogue: kernel/time/timeconv.c
 *
 * Converts between raw epoch seconds and broken-down struct tm representation.
 */

#include "time/galio_time.h"

#define SECS_PER_HOUR   GALIO_SECS_PER_HOUR
#define SECS_PER_DAY    GALIO_SECS_PER_DAY

/* Math helpers */
static s64 div_s64_rem(s64 dividend, s32 divisor, s32 *remainder)
{
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static u64 div64_u64_rem(u64 dividend, u64 divisor, u64 *remainder)
{
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static u32 upper_32_bits(u64 n)
{
    return (u32)(n >> 32);
}

static u32 lower_32_bits(u64 n)
{
    return (u32)n;
}

/* ------------------------------------------------------------------
 * galio_time64_to_tm - converts calendar time to broken-down time.
 * Linux equivalent: time64_to_tm()
 * ------------------------------------------------------------------ */
void galio_time64_to_tm(s64 totalsecs, int offset, galio_tm *result)
{
    u32 u32tmp, day_of_century, year_of_century, day_of_year, month, day;
    u64 u64tmp, udays, century, year;
    bool is_Jan_or_Feb, is_leap_year;
    long days, rem;
    s32 remainder;

    days = div_s64_rem(totalsecs, SECS_PER_DAY, &remainder);
    rem = remainder;
    rem += offset;
    while (rem < 0) {
        rem += SECS_PER_DAY;
        --days;
    }
    while (rem >= SECS_PER_DAY) {
        rem -= SECS_PER_DAY;
        ++days;
    }

    result->tm_hour = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    result->tm_min = rem / 60;
    result->tm_sec = rem % 60;

    /* January 1, 1970 was a Thursday. */
    result->tm_wday = (4 + days) % 7;
    if (result->tm_wday < 0)
        result->tm_wday += 7;

    udays = ((u64)days) + 2305843009213814918ULL;

    u64tmp = 4 * udays + 3;
    century = div64_u64_rem(u64tmp, 146097, &u64tmp);
    day_of_century = (u32)(u64tmp / 4);

    u32tmp = 4 * day_of_century + 3;
    u64tmp = 2939745ULL * u32tmp;
    year_of_century = upper_32_bits(u64tmp);
    day_of_year = lower_32_bits(u64tmp) / 2939745 / 4;

    year = 100 * century + year_of_century;
    is_leap_year = year_of_century ? !(year_of_century % 4) : !(century % 4);

    u32tmp = 2141 * day_of_year + 132377;
    month = u32tmp >> 16;
    day = ((u16)u32tmp) / 2141;

    is_Jan_or_Feb = day_of_year >= 306;

    year = year + is_Jan_or_Feb - 6313183731940000ULL;
    month = is_Jan_or_Feb ? month - 12 : month;
    day = day + 1;
    day_of_year += is_Jan_or_Feb ? -306 : 31 + 28 + is_leap_year;

    result->tm_year = (long)(year - 1900);
    result->tm_mon  = (int)month;
    result->tm_mday = (int)day;
    result->tm_yday = (int)day_of_year;
}

/* ------------------------------------------------------------------
 * galio_ns_to_timespec64 / galio_timespec64_to_ns
 * Linux equivalent: ns_to_timespec64() / timespec64_to_ns()
 * ------------------------------------------------------------------ */
galio_timespec64 galio_ns_to_timespec64(s64 ns)
{
    galio_timespec64 ts;
    s32 rem;
    
    if (ns < 0) {
        ts.tv_sec = div_s64_rem(ns - GALIO_NSEC_PER_SEC + 1, GALIO_NSEC_PER_SEC, &rem);
        ts.tv_nsec = GALIO_NSEC_PER_SEC + rem;
    } else {
        ts.tv_sec = div_s64_rem(ns, GALIO_NSEC_PER_SEC, &rem);
        ts.tv_nsec = rem;
    }
    return ts;
}

s64 galio_timespec64_to_ns(const galio_timespec64 *ts)
{
    return (ts->tv_sec * GALIO_NSEC_PER_SEC) + ts->tv_nsec;
}

/* Note: galio_tm_to_time64() omitted for brevity; can be implemented if needed. */
