/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * sleep_timeout.c - Sleep and delay helpers for the Galio kernel.
 *
 * Linux analogue: kernel/time/sleep_timeout.c
 *
 * Provides msleep, usleep, and nsleep functions based on jiffies and hrtimers.
 * Because Galio's current scheduler is still evolving, these do a simple
 * busy-wait loop. A full implementation would yield to the scheduler.
 */

#include "time/galio_time.h"
#include "arch/x86/cpu.h" /* For cpu_pause() or hlt */

/* ------------------------------------------------------------------
 * galio_msleep - sleep safely (in ms)
 * Linux equivalent: msleep()
 * ------------------------------------------------------------------ */
void galio_msleep(u32 msecs)
{
    u64 target = galio_jiffies_read() + galio_msecs_to_jiffies(msecs);
    
    /* In a real preemptive kernel, this would block the thread.
     * For now, we spin/halt until jiffies catches up. */
    while (galio_time_before(galio_jiffies_read(), target)) {
        /* Enable interrupts to allow PIT to fire, then halt CPU */
        asm volatile("sti; hlt");
    }
}

/* ------------------------------------------------------------------
 * galio_usleep / galio_nsleep
 * Linux equivalent: usleep_range() / ndelay()
 * ------------------------------------------------------------------ */
void galio_usleep(u32 usecs)
{
    u64 target_ns = galio_ktime_get() + (u64)usecs * GALIO_NSEC_PER_USEC;
    
    while (galio_ktime_get() < target_ns) {
        asm volatile("pause");
    }
}

void galio_nsleep(u64 nsecs)
{
    u64 target_ns = galio_ktime_get() + nsecs;
    
    while (galio_ktime_get() < target_ns) {
        asm volatile("pause");
    }
}
