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
