/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * jiffies.c - Jiffies tick counter for the Galio kernel.
 *
 * Linux analogue: kernel/time/jiffies.c
 *
 * Linux's jiffies are a globally shared counter incremented on every timer
 * interrupt. Here we build the same abstraction on top of Galio's PIT driver
 * (pit_get_ticks()), which increments at GALIO_HZ (100) per second.
 *
 * Galio differences from Linux:
 *  - No seqcount locking (single-CPU kernel, preemption disabled in IRQ).
 *  - galio_jiffies_read() wraps pit_get_ticks() for clocksource compatibility.
 *  - Conversion helpers (msecs_to_jiffies / jiffies_to_msecs) are inlined
 *    here rather than living in a separate header to avoid header sprawl.
 */

#include "time/galio_time.h"
#include "drivers/pit.h"
#include "arch/x86/cpu.h"

/* ------------------------------------------------------------------
 * Internal: read the raw 64-bit jiffy counter from the PIT driver.
 * Linux equivalent: jiffies_read() (clocksource callback).
 * ------------------------------------------------------------------ */
u64 galio_jiffies_read(void)
{
    return (u64)pit_get_ticks();
}

/* ------------------------------------------------------------------
 * galio_jiffies_to_msecs - convert jiffies to milliseconds.
 * Linux equivalent: jiffies_to_msecs() in <linux/jiffies.h>.
 * ------------------------------------------------------------------ */
u64 galio_jiffies_to_msecs(u64 j)
{
    return j * (u64)GALIO_MSEC_PER_HZ;
}

/* ------------------------------------------------------------------
 * galio_msecs_to_jiffies - convert milliseconds to jiffies.
 * Linux equivalent: msecs_to_jiffies() in <linux/jiffies.h>.
 * Rounds up to guarantee at-least the requested delay.
 * ------------------------------------------------------------------ */
u64 galio_msecs_to_jiffies(u64 ms)
{
    /* Ceiling division: (ms * HZ + 999) / 1000 */
    return (ms * (u64)GALIO_HZ + 999u) / 1000u;
}

/* ------------------------------------------------------------------
 * galio_jiffies_to_usecs / galio_usecs_to_jiffies
 * Linux equivalent: jiffies_to_usecs() / usecs_to_jiffies().
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
 * Linux equivalent: time_after() / time_before() in <linux/jiffies.h>.
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
 * Kept for symmetry with Linux's clocksource_default_clock().
 * ------------------------------------------------------------------ */
void galio_jiffies_init(void)
{
    /* The jiffies clocksource is backed by the PIT driver, which is
     * initialised in kmain.c before the time subsystem. No extra work
     * is required here.  In Linux this would call
     * __clocksource_register(&clocksource_jiffies). */
}
