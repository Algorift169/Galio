/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * sched_clock.c - Scheduler clock for the Galio kernel.
 *
 * Linux analogue: kernel/time/sched_clock.c
 *
 * Provides a fast, lockless, monotonically increasing nanosecond clock
 * primarily intended for scheduler time accounting.
 */

#include "time/galio_time.h"

/* ------------------------------------------------------------------
 * galio_sched_clock_ns - returns monotonic ns for scheduler.
 * Linux equivalent: sched_clock()
 * ------------------------------------------------------------------ */
u64 galio_sched_clock_ns(void)
{
    /* For Galio, this just maps to ktime_get_boot_ns() which is fast enough.
     * In Linux, this uses a specialized seqcount-latched fast path reading
     * directly from the TSC or an equivalent fast clocksource. */
    return galio_ktime_get_boot_ns();
}

/* ------------------------------------------------------------------
 * galio_sched_clock_init
 * ------------------------------------------------------------------ */
void galio_sched_clock_init(void)
{
    /* No special init needed currently */
}
