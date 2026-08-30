/* SPDX-License-Identifier: GPL-2.0 (adapted for Galio freestanding kernel)
 *
 * clockevents.c - Clock event device abstraction for the Galio kernel.
 *
 * Linux analogue: kernel/time/clockevents.c
 *
 * Handles the registration of hardware timers (e.g., PIT, APIC timer)
 * and dispatches their ticks to the high-level timing subsystems (ktimer/hrtimer).
 */

#include "time/galio_time.h"

static galio_clock_event_t *active_clock_event = NULL;

/* ------------------------------------------------------------------
 * galio_clockevents_register - register a clock event device.
 * Linux equivalent: clockevents_register_device()
 * ------------------------------------------------------------------ */
void galio_clockevents_register(galio_clock_event_t *dev)
{
    if (!dev || !dev->handler) {
        return;
    }
    
    /* Simple replacement policy for best rating */
    if (!active_clock_event || dev->rating > active_clock_event->rating) {
        active_clock_event = dev;
    }
}

/* ------------------------------------------------------------------
 * galio_clockevents_tick - the main tick handler.
 * Called from the hardware timer interrupt (e.g., PIT).
 * Linux equivalent: tick_handle_periodic() / tick_handle_oneshot()
 * ------------------------------------------------------------------ */
void galio_clockevents_tick(void)
{
    /* 1. Advance kernel time */
    kernel_time_update();

    /* 2. Run ktimer queues */
    galio_ktimer_run_pending();

    /* 3. Run hrtimer queues */
    galio_hrtimer_run_queues();
}

/* ------------------------------------------------------------------
 * galio_clockevents_init - subsystem initialization.
 * ------------------------------------------------------------------ */
void galio_clockevents_init(void)
{
    /* Handled by device drivers pushing to register */
}
