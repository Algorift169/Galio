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
 * clockevents.c - Clock event device abstraction for the Galio kernel.
 *
 * Handles the registration of hardware timers (e.g., PIT, APIC timer)
 * and dispatches their ticks to the high-level timing subsystems (ktimer/hrtimer).
 */

#include "time/galio_time.h"

static galio_clock_event_t *active_clock_event = NULL;

/* ------------------------------------------------------------------
 * galio_clockevents_register - register a clock event device.
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
