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
#include "process/pcb.h"

static volatile u64 accounting_total_ticks;
static volatile u64 accounting_idle_ticks;
static u64 scheduler_tsc_hz;
static u64 scheduler_last_tsc;
static u8 scheduler_accounting_ready;

static u64 scheduler_read_tsc(void) {
    u32 low;
    u32 high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((u64)high << 32) | low;
}

// Calibrate TSC frequency using PIT timer
static void scheduler_calibrate_tsc(void) {
    u32 start_ticks = pit_get_ticks();
    u64 start_tsc = scheduler_read_tsc();
    u32 elapsed_ticks;
    u64 elapsed_tsc;

    // Wait for 50 PIT ticks (approx. 50 ms) to measure TSC frequency
    while ((u32)(pit_get_ticks() - start_ticks) < 50u) {
        __asm__ volatile("hlt");
    }

    // Calculate elapsed ticks and TSC cycles
    elapsed_ticks = pit_get_ticks() - start_ticks;
    elapsed_tsc = scheduler_read_tsc() - start_tsc;
    if (elapsed_ticks == 0u || elapsed_tsc == 0u) {
        scheduler_tsc_hz = 0u;
        scheduler_last_tsc = scheduler_read_tsc();
        scheduler_accounting_ready = 1u;
        kprintf("Scheduler: TSC calibration unavailable; using PIT accounting\n");
        return;
    }

    scheduler_tsc_hz = (elapsed_tsc * GALIO_HZ) / elapsed_ticks;
    scheduler_last_tsc = scheduler_read_tsc();
    scheduler_accounting_ready = 1u;
    kprintf("Scheduler: calibrated TSC=%u MHz\n",
            (u32)(scheduler_tsc_hz / 1000000u));
}

void process_accounting_tick(void) {
    process_t *current = process_current();
    u64 now_tsc = scheduler_read_tsc();

    if (!scheduler_accounting_ready) return;

    u64 elapsed_tsc = now_tsc - scheduler_last_tsc;

    scheduler_last_tsc = now_tsc;
    if (scheduler_tsc_hz == 0u) elapsed_tsc = 1u;

    accounting_total_ticks += elapsed_tsc;
    if (current) {
        current->runtime_ticks += elapsed_tsc;
        if (current->accounting_idle) accounting_idle_ticks += elapsed_tsc;
    }
}

void process_accounting_set_idle(u8 idle) {
    process_t *current = process_current();

    if (current) {
        current->accounting_idle = idle ? 1u : 0u;
    }
}

/* Scheduler tick handler - called by PIT and performs preemption */
void scheduler_tick(registers_t *regs) {
    process_t *current = process_current();
    // Poll wait queues for timeouts and update accounting
    process_wait_queue_poll_timeouts(pit_get_ticks());
    process_balance_runqueues();
    if (current) {
        process_accounting_tick();
        pcb_accounting_tick(current);

        if (regs && (regs->cs & 3) == 3 && current->time_slice == 0 &&
            (current->state == PROCESS_RUNNING || current->state == PROCESS_ZOMBIE ||
             current->state == PROCESS_WAITING)) {
            current->time_slice = PROCESS_TIME_SLICE;
            process_preempt(regs);
        }
    }
}

/* Initialize scheduler - preemptive round-robin mode */
void scheduler_init(void) {
    pit_install_callback(scheduler_tick);
    // Calibrate TSC frequency for accounting if available
    scheduler_calibrate_tsc();
    kprintf("Scheduler: preemptive round-robin, 8 priority levels, FIFO queues, 10 ms slice\n");
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
