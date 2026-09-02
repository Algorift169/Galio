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
 * jiffies.c - Jiffies tick counter for the Galio kernel.
 *
 * The jiffies counter is a globally shared tick value incremented on each timer
 * interrupt. This implementation builds the abstraction on top of the PIT driver
 * (pit_get_ticks()), which increments at GALIO_HZ (100) per second.
 */

#include "time/galio_time.h"
#include "drivers/pit.h"
#include "arch/x86/cpu.h"

/* ------------------------------------------------------------------
 * Internal: read the raw 64-bit jiffy counter from the PIT driver.
 * ------------------------------------------------------------------ */
u64 galio_jiffies_read(void)
{
    return (u64)pit_get_ticks();
}

/* ------------------------------------------------------------------
 * galio_jiffies_to_msecs - convert jiffies to milliseconds.
 * ------------------------------------------------------------------ */
u64 galio_jiffies_to_msecs(u64 j)
{
    return j * (u64)GALIO_MSEC_PER_HZ;
}

/* ------------------------------------------------------------------
 * galio_msecs_to_jiffies - convert milliseconds to jiffies.
 * Rounds up to guarantee at-least the requested delay.
 * ------------------------------------------------------------------ */
u64 galio_msecs_to_jiffies(u64 ms)
{
    /* Ceiling division: (ms * HZ + 999) / 1000 */
    return (ms * (u64)GALIO_HZ + 999u) / 1000u;
}

/* ------------------------------------------------------------------
 * galio_jiffies_to_usecs / galio_usecs_to_jiffies
 * ------------------------------------------------------------------ */
u64 galio_jiffies_to_usecs(u64 j)
{
    return j * (1000000u / GALIO_HZ);
}

u64 galio_usecs_to_jiffies(u64 us)
{
    return (us * (u64)GALIO_HZ + 999999u) / 1000000u;
}

/* ------------------------------------------------------------------
 * galio_time_after / galio_time_before
 * Uses unsigned subtraction to handle 64-bit wraparound.
 * ------------------------------------------------------------------ */
int galio_time_after(u64 a, u64 b)
{
    return (s64)(a - b) > 0;
}

int galio_time_before(u64 a, u64 b)
{
    return galio_time_after(b, a);
}

/* ------------------------------------------------------------------
 * galio_jiffies_init - nothing to do: PIT initialises jiffies for us.
 * ------------------------------------------------------------------ */
void galio_jiffies_init(void)
{
    /* The jiffies clocksource is backed by the PIT driver, which is
     * initialised in kmain.c before the time subsystem. No extra work
     * is required here.  In Linux this would call
     * __clocksource_register(&clocksource_jiffies). */
}
