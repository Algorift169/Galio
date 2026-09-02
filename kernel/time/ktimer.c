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
 * ktimer.c - Low-resolution kernel timers for Galio.
 *
 * Linux analogue: kernel/time/timer.c (struct timer_list)
 *
 * Implements a simple linked-list based timer queue driven by jiffies.
 * In Linux, this uses a complex timer wheel. For Galio's scale, an ordered
 * linked list is sufficient.
 */

#include "time/galio_time.h"
#include "arch/x86/cpu.h" /* irq_save/restore */

static galio_ktimer_t *ktimer_list = NULL;

/* ------------------------------------------------------------------
 * galio_ktimer_init - initialize a timer structure.
 * Linux equivalent: timer_setup()
 * ------------------------------------------------------------------ */
void galio_ktimer_init(galio_ktimer_t *t, galio_ktimer_fn fn)
{
    if (t) {
        t->function = fn;
        t->flags = 0;
        t->next = NULL;
    }
}

/* ------------------------------------------------------------------
 * galio_ktimer_start - enqueue a timer to expire at absolute jiffies.
 * Linux equivalent: mod_timer()
 * ------------------------------------------------------------------ */
void galio_ktimer_start(galio_ktimer_t *t, u64 expires_jiffies)
{
    if (!t) return;

    u64 flags = irq_save();
    
    /* Remove it if it's already queued */
    galio_ktimer_cancel(t);

    t->expires = expires_jiffies;

    /* Insert in sorted order (earliest expiry first) */
    if (!ktimer_list || galio_time_before(t->expires, ktimer_list->expires)) {
        t->next = ktimer_list;
        ktimer_list = t;
    } else {
        galio_ktimer_t *curr = ktimer_list;
        while (curr->next && galio_time_after(t->expires, curr->next->expires)) {
            curr = curr->next;
        }
        t->next = curr->next;
        curr->next = t;
    }
    
    t->flags |= 1; /* Mark as active */

    irq_restore(flags);
}

void galio_ktimer_start_ms(galio_ktimer_t *t, u64 delay_ms)
{
    galio_ktimer_start(t, galio_jiffies_read() + galio_msecs_to_jiffies(delay_ms));
}

/* ------------------------------------------------------------------
 * galio_ktimer_cancel - dequeue a timer.
 * Linux equivalent: del_timer()
 * ------------------------------------------------------------------ */
void galio_ktimer_cancel(galio_ktimer_t *t)
{
    if (!t || !(t->flags & 1)) return;

    u64 flags = irq_save();

    if (ktimer_list == t) {
        ktimer_list = t->next;
    } else {
        galio_ktimer_t *curr = ktimer_list;
        while (curr && curr->next != t) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = t->next;
        }
    }
    
    t->next = NULL;
    t->flags &= ~1; /* Clear active flag */

    irq_restore(flags);
}

int galio_ktimer_pending(const galio_ktimer_t *t)
{
    return t && (t->flags & 1);
}

/* ------------------------------------------------------------------
 * galio_ktimer_run_pending - process expired timers.
 * Called from clockevents_tick()
 * Linux equivalent: run_timer_softirq()
 * ------------------------------------------------------------------ */
void galio_ktimer_run_pending(void)
{
    u64 now = galio_jiffies_read();
    
    u64 flags = irq_save();
    
    while (ktimer_list && galio_time_after(now, ktimer_list->expires)) {
        galio_ktimer_t *t = ktimer_list;
        ktimer_list = t->next;
        
        t->next = NULL;
        t->flags &= ~1;
        
        irq_restore(flags);
        
        /* Execute the callback with interrupts enabled */
        if (t->function) {
            t->function(t);
        }
        
        flags = irq_save();
    }
    
    irq_restore(flags);
}

void galio_ktimer_subsystem_init(void)
{
    ktimer_list = NULL;
}
