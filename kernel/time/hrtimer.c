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
 * hrtimer.c - High-resolution timers for Galio.
 *
 * Implements a queue of timers driven by nanosecond precision monotonic time.
 * For Galio, we use an ordered list polled by the tick handler (or clockevent).
 */

#include "time/galio_time.h"
#include "arch/x86/cpu.h" /* irq_save/restore */

static galio_hrtimer_t *hrtimer_list = NULL;

/* ------------------------------------------------------------------
 * galio_hrtimer_init
 * ------------------------------------------------------------------ */
void galio_hrtimer_init(galio_hrtimer_t *timer, galio_hrtimer_fn fn)
{
    if (timer) {
        timer->function = fn;
        timer->active = 0;
        timer->next = NULL;
    }
}

/* ------------------------------------------------------------------
 * galio_hrtimer_start
 * ------------------------------------------------------------------ */
void galio_hrtimer_start(galio_hrtimer_t *timer, galio_ktime_t expires_ns)
{
    if (!timer) return;

    u64 flags = irq_save();
    
    galio_hrtimer_cancel(timer);

    timer->_softexpires = expires_ns;

    if (!hrtimer_list || timer->_softexpires < hrtimer_list->_softexpires) {
        timer->next = hrtimer_list;
        hrtimer_list = timer;
    } else {
        galio_hrtimer_t *curr = hrtimer_list;
        while (curr->next && timer->_softexpires >= curr->next->_softexpires) {
            curr = curr->next;
        }
        timer->next = curr->next;
        curr->next = timer;
    }
    
    timer->active = 1;

    irq_restore(flags);
}

void galio_hrtimer_start_ms(galio_hrtimer_t *timer, u64 ms)
{
    galio_hrtimer_start(timer, galio_ktime_get() + galio_ms_to_ktime(ms));
}

/* ------------------------------------------------------------------
 * galio_hrtimer_cancel
 * ------------------------------------------------------------------ */
void galio_hrtimer_cancel(galio_hrtimer_t *timer)
{
    if (!timer || !timer->active) return;

    u64 flags = irq_save();

    if (hrtimer_list == timer) {
        hrtimer_list = timer->next;
    } else {
        galio_hrtimer_t *curr = hrtimer_list;
        while (curr && curr->next != timer) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = timer->next;
        }
    }
    
    timer->next = NULL;
    timer->active = 0;

    irq_restore(flags);
}

int galio_hrtimer_active(const galio_hrtimer_t *timer)
{
    return timer && timer->active;
}

/* ------------------------------------------------------------------
 * galio_hrtimer_run_queues - process expired timers.
 * Called from clockevents_tick().
 * ------------------------------------------------------------------ */
void galio_hrtimer_run_queues(void)
{
    galio_ktime_t now = galio_ktime_get();
    
    u64 flags = irq_save();
    
    while (hrtimer_list && now >= hrtimer_list->_softexpires) {
        galio_hrtimer_t *timer = hrtimer_list;
        hrtimer_list = timer->next;
        
        timer->next = NULL;
        timer->active = 0;
        
        irq_restore(flags);
        
        /* Execute the callback with interrupts enabled */
        if (timer->function) {
            galio_hrtimer_restart_t restart = timer->function(timer);
            if (restart == GALIO_HRTIMER_RESTART) {
                /* The timer callback is responsible for updating _softexpires */
                galio_hrtimer_start(timer, timer->_softexpires);
            }
        }
        
        flags = irq_save();
    }
    
    irq_restore(flags);
}

void galio_hrtimer_subsystem_init(void)
{
    hrtimer_list = NULL;
}
