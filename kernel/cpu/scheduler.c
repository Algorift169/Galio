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

#include "cpu/scheduler.h"
#include "process.h"
#include "pit.h"
#include "kprintf.h"

void cpu_scheduler_tick(registers_t *regs) {
    process_t *current = process_current();
    if (!current) {
        return;
    }

    process_accounting_tick();
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

void cpu_scheduler_init(void) {
    pit_install_callback(cpu_scheduler_tick);
    kprintf("CPU scheduler initialized (Shortest Job First job scheduler)\n");
    kprintf("  - burst time used to choose shortest ready process\n");
    kprintf("  - time slice = %u\n", PROCESS_TIME_SLICE);
}
