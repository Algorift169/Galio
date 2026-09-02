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

/* scheduler.c - Preemptive scheduler using PIT timer */

#include "process.h"
#include "pit.h"
#include "kprintf.h"

static volatile u64 accounting_total_ticks;
static volatile u64 accounting_idle_ticks;
static volatile u8 accounting_idle_active;

void process_accounting_tick(void) {
    accounting_total_ticks++;
    if (accounting_idle_active) accounting_idle_ticks++;
}

void process_accounting_set_idle(u8 idle) {
    accounting_idle_active = idle ? 1 : 0;
}

/* Scheduler tick handler - called by PIT and performs preemption */
void scheduler_tick(registers_t *regs) {
    process_t *current = process_current();
    if (current) {
        current->ticks++;

        if (current->time_slice > 0) {
            current->time_slice--;
        }

        if (regs && (regs->cs & 3) == 3 &&
            current->time_slice == 0 && current->state == PROCESS_RUNNING) {
            current->time_slice = PROCESS_TIME_SLICE;
            process_preempt(regs);
        }
    }
}

/* Initialize scheduler - preemptive round-robin mode */
void scheduler_init(void) {
    pit_install_callback(scheduler_tick);
    kprintf("Scheduler initialized (preemptive mode)\n");
    kprintf("  - Timer running at 100Hz with time slice %u\n", PROCESS_TIME_SLICE);
    kprintf("  - Context switches on yield() or when a process exhausts its slice\n");
}

/* CPU statistics - calculate from all processes via process_t accessors */
u64 process_get_total_ticks(void) {
    return accounting_total_ticks;
}

u64 process_get_idle_ticks(void) {
    return accounting_idle_ticks;
}

u8 process_get_cpu_usage(void) {
    extern u32 pit_get_ticks(void);
    static u32 last_update_ticks = 0;
    static u8 cached_usage = 0;
    static u64 last_total = 0;
    static u64 last_idle = 0;

    u32 now = pit_get_ticks();
    if (last_update_ticks == 0 || (now - last_update_ticks) >= 100) {
        u32 total = process_get_total_ticks();
        u32 idle = process_get_idle_ticks();
        
        u64 delta_total = total - last_total;
        u64 delta_idle = idle - last_idle;

        last_total = total;
        last_idle = idle;
        last_update_ticks = now;

        if (delta_total > 0) {
            if (delta_idle > delta_total) delta_idle = delta_total;
            cached_usage = (u8)(((delta_total - delta_idle) * 100) / delta_total);
        }
    }
    return cached_usage;
}
