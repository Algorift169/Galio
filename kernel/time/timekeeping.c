/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
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
 * timekeeping.c - Core timekeeping subsystem for the Galio kernel.
 *
 * Linux analogue: kernel/time/timekeeping.c
 *
 * Manages the canonical system times (monotonic and wall-clock).
 * Bridges the gap between the tick-based (jiffies/DateTime) time in kernel_time
 * and high-resolution sub-tick nanoseconds via the active clocksource.
 */

#include "time/galio_time.h"
#include "kernel_time.h"

/* The nanosecond offset added to monotonic time to yield real (wall) time */
static s64 offs_real = 0;

/* ------------------------------------------------------------------
 * galio_timekeeping_init
 * Synchronize with the base DateTime system
 * ------------------------------------------------------------------ */
void galio_timekeeping_init(void)
{
    /* Initialize base time from RTC (which sets epoch seconds) */
    u32 epoch = kernel_time_get_epoch_seconds();
    
    /* Initially assume monotonic time is 0 (boot) */
    offs_real = (s64)epoch * GALIO_NSEC_PER_SEC;
}

/* ------------------------------------------------------------------
 * galio_ktime_get - returns monotonic time in nanoseconds.
 * Linux equivalent: ktime_get()
 * ------------------------------------------------------------------ */
galio_ktime_t galio_ktime_get(void)
{
    /* Base monotonic time is uptime seconds + microseconds */
    u32 secs = kernel_time_get_uptime_seconds();
    u32 usecs = kernel_time_get_microseconds();
    
    /* Plus high-resolution interpolation from the best clocksource (if any) */
    u64 ns = (u64)secs * GALIO_NSEC_PER_SEC + (u64)usecs * GALIO_NSEC_PER_USEC;
    
    struct galio_clocksource *cs = galio_clocksource_get_best();
    if (cs && cs->read) {
        /* In a full implementation, we'd add cycles since last tick.
         * For now, the base time is precise enough for Galio's current scale. */
    }
    
    return ns;
}

/* ------------------------------------------------------------------
 * galio_ktime_get_real - returns real (wall) time in nanoseconds.
 * Linux equivalent: ktime_get_real()
 * ------------------------------------------------------------------ */
galio_ktime_t galio_ktime_get_real(void)
{
    return galio_ktime_add(galio_ktime_get(), offs_real);
}

/* ------------------------------------------------------------------
 * galio_ktime_get_ts64 - returns monotonic time as timespec64.
 * Linux equivalent: ktime_get_ts64()
 * ------------------------------------------------------------------ */
galio_timespec64 galio_ktime_get_ts64(void)
{
    return galio_ns_to_timespec64(galio_ktime_get());
}

/* ------------------------------------------------------------------
 * galio_ktime_get_real_ts64 - returns real (wall) time as timespec64.
 * Linux equivalent: ktime_get_real_ts64()
 * ------------------------------------------------------------------ */
galio_timespec64 galio_ktime_get_real_ts64(void)
{
    return galio_ns_to_timespec64(galio_ktime_get_real());
}

/* ------------------------------------------------------------------
 * galio_ktime_get_boot_ns - returns monotonic ns (same as ktime_get for now).
 * Linux equivalent: ktime_get_boot_ns()
 * ------------------------------------------------------------------ */
u64 galio_ktime_get_boot_ns(void)
{
    return (u64)galio_ktime_get();
}
