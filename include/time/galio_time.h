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
 * galio_time.h - Master header for the Galio kernel time subsystem.
 *
 * This header provides the Galio equivalents of common kernel timing concepts,
 * including nanosecond timestamps, jiffies, clock abstractions, and timers.
 */

#ifndef GALIO_TIME_H
#define GALIO_TIME_H

#include "common.h"
#include "kernel_time.h"   /* DateTime, kernel_time_get_epoch_seconds() */

/* =========================================================
 * 1. Basic time types
 * ========================================================= */

/* 64-bit nanosecond timestamp (monotonic). */
typedef s64 galio_ktime_t;

/* Broken-down time with nanosecond precision. */
typedef struct {
    s64 tv_sec;   /* seconds since epoch */
    s32 tv_nsec;  /* nanoseconds [0, 999999999] */
} galio_timespec64;

/* Day-of-week/year extension of DateTime. */
typedef struct {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;   /* months since January [0,11] */
    long tm_year; /* years since 1900 */
    int tm_wday;  /* days since Sunday [0,6] */
    int tm_yday;  /* days since January 1 [0,365] */
} galio_tm;

#define GALIO_NSEC_PER_SEC   1000000000LL
#define GALIO_NSEC_PER_MSEC  1000000LL
#define GALIO_NSEC_PER_USEC  1000LL
#define GALIO_USEC_PER_SEC   1000000LL
#define GALIO_MSEC_PER_SEC   1000LL
#define GALIO_SECS_PER_HOUR  3600
#define GALIO_SECS_PER_DAY   86400

static inline galio_ktime_t galio_ktime_set(s64 secs, u32 nsecs)
{
    return secs * GALIO_NSEC_PER_SEC + (galio_ktime_t)nsecs;
}

static inline galio_ktime_t galio_ktime_add(galio_ktime_t a, galio_ktime_t b)
{
    return a + b;
}

static inline galio_ktime_t galio_ktime_sub(galio_ktime_t a, galio_ktime_t b)
{
    return a - b;
}

static inline galio_ktime_t galio_ms_to_ktime(u64 ms)
{
    return (galio_ktime_t)(ms * (u64)GALIO_NSEC_PER_MSEC);
}

static inline galio_ktime_t galio_us_to_ktime(u64 us)
{
    return (galio_ktime_t)(us * (u64)GALIO_NSEC_PER_USEC);
}

static inline s64 galio_ktime_to_ms(galio_ktime_t kt)
{
    return kt / GALIO_NSEC_PER_MSEC;
}

static inline s64 galio_ktime_to_us(galio_ktime_t kt)
{
    return kt / GALIO_NSEC_PER_USEC;
}

static inline s64 galio_ktime_to_ns(galio_ktime_t kt)
{
    return (s64)kt;
}

/* =========================================================
 * 2. Jiffies
 * ========================================================= */

#define GALIO_HZ           100u   /* PIT configured at 100 Hz */
#define GALIO_MSEC_PER_HZ  (1000u / GALIO_HZ)

/* jiffies_read() and jiffies_64 */
u64 galio_jiffies_read(void);
u64 galio_jiffies_to_msecs(u64 j);
u64 galio_msecs_to_jiffies(u64 ms);
u64 galio_jiffies_to_usecs(u64 j);
u64 galio_usecs_to_jiffies(u64 us);
int  galio_time_after(u64 a, u64 b);   /* a > b (wraps safely) */
int  galio_time_before(u64 a, u64 b);  /* a < b */
void galio_jiffies_init(void);

/* =========================================================
 * 3. Clocksource abstraction
 * ========================================================= */

#define GALIO_CLOCKSOURCE_NAME_LEN 32

struct galio_clocksource {
    char   name[GALIO_CLOCKSOURCE_NAME_LEN];
    u32    rating;          /* quality: 1=lowest, 400=best */
    u64  (*read)(struct galio_clocksource *cs);
    u64    mask;            /* bit mask for counter */
    u32    mult;            /* cycles-to-nsec multiplier */
    u32    shift;           /* cycles-to-nsec shift */
    u64    max_cycles;      /* max safe delta for overflow */
    /* private */
    u64    cycle_last;
};

void galio_clocksource_register(struct galio_clocksource *cs);
struct galio_clocksource *galio_clocksource_get_best(void);
u64  galio_clocksource_cyc2ns(struct galio_clocksource *cs, u64 cycles);
void galio_clocksource_init(void);

/* =========================================================
 * 4. Clock-event abstraction
 * ========================================================= */

typedef void (*galio_clock_event_handler_t)(void *data);

#define GALIO_CLOCK_EVT_NAME_LEN 32

typedef struct {
    char  name[GALIO_CLOCK_EVT_NAME_LEN];
    u32   rating;
    u64   period_ns;     /* period in nanoseconds */
    galio_clock_event_handler_t handler;
    void *handler_data;
} galio_clock_event_t;

void galio_clockevents_register(galio_clock_event_t *dev);
void galio_clockevents_tick(void);         /* call from PIT IRQ */
void galio_clockevents_init(void);

/* =========================================================
 * 5. Low-resolution kernel timers
 * ========================================================= */

struct galio_ktimer;
typedef void (*galio_ktimer_fn)(struct galio_ktimer *);

typedef struct galio_ktimer {
    u64              expires;      /* jiffies tick when this fires */
    galio_ktimer_fn  function;
    u32              flags;
    struct galio_ktimer *next;     /* intrusive singly-linked list */
} galio_ktimer_t;

void galio_ktimer_init(galio_ktimer_t *t, galio_ktimer_fn fn);
void galio_ktimer_start(galio_ktimer_t *t, u64 expires_jiffies);
void galio_ktimer_start_ms(galio_ktimer_t *t, u64 delay_ms);
void galio_ktimer_cancel(galio_ktimer_t *t);
int  galio_ktimer_pending(const galio_ktimer_t *t);
void galio_ktimer_run_pending(void);   /* called from PIT tick */
void galio_ktimer_subsystem_init(void);

/* =========================================================
 * 6. High-resolution timers
 * ========================================================= */

typedef enum {
    GALIO_HRTIMER_NORESTART = 0,
    GALIO_HRTIMER_RESTART   = 1,
} galio_hrtimer_restart_t;

struct galio_hrtimer;
typedef galio_hrtimer_restart_t (*galio_hrtimer_fn)(struct galio_hrtimer *);

typedef struct galio_hrtimer {
    galio_ktime_t      _softexpires;  /* expiry time (ns, monotonic) */
    galio_hrtimer_fn   function;
    u8                 active;
    struct galio_hrtimer *next;
} galio_hrtimer_t;

void galio_hrtimer_init(galio_hrtimer_t *timer, galio_hrtimer_fn fn);
void galio_hrtimer_start(galio_hrtimer_t *timer, galio_ktime_t expires_ns);
void galio_hrtimer_start_ms(galio_hrtimer_t *timer, u64 ms);
void galio_hrtimer_cancel(galio_hrtimer_t *timer);
int  galio_hrtimer_active(const galio_hrtimer_t *timer);
void galio_hrtimer_run_queues(void);   /* call from PIT tick */
void galio_hrtimer_subsystem_init(void);

/* =========================================================
 * 7. Monotonic / wall-clock reads
 * ========================================================= */

galio_ktime_t    galio_ktime_get(void);          /* monotonic ns since boot */
galio_ktime_t    galio_ktime_get_real(void);     /* wall-clock ns since epoch */
galio_timespec64 galio_ktime_get_ts64(void);     /* monotonic as timespec64 */
galio_timespec64 galio_ktime_get_real_ts64(void);/* wall-clock as timespec64 */
u64              galio_ktime_get_boot_ns(void);  /* boot time in ns */
void             galio_timekeeping_init(void);

/* =========================================================
 * 8. Scheduler clock
 * ========================================================= */

u64 galio_sched_clock_ns(void);   /* monotonic ns, cheap, for scheduler use */
void galio_sched_clock_init(void);

/* =========================================================
 * 9. Time conversion utilities
 * ========================================================= */

void galio_time64_to_tm(s64 totalsecs, int offset, galio_tm *result);
s64  galio_tm_to_time64(const galio_tm *tm);
galio_timespec64 galio_ns_to_timespec64(s64 ns);
s64  galio_timespec64_to_ns(const galio_timespec64 *ts);

/* =========================================================
 * 10. Sleep helpers
 * ========================================================= */

void galio_msleep(u32 msecs);
void galio_usleep(u32 usecs);
void galio_nsleep(u64 nsecs);

#endif /* GALIO_TIME_H */
