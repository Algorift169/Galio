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

/* scheduler_test.c - Basic scheduler tests */
#include "kprintf.h"
#include "process.h"
#include "signals.h"

static void scheduler_dummy(void) {
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void scheduler_test(void) {
    kprintf("[KTEST] scheduler_test\n");

    process_t *current = process_current();
    if (!current) {
        kprintf("[KTEST FAIL] scheduler_test: current process unavailable\n");
        return;
    }

    u32 pid = process_create(scheduler_dummy, 1);
    if (!pid) {
        kprintf("[KTEST FAIL] scheduler_test: process_create failed\n");
        return;
    }

    process_t *child = process_get(pid);
    if (!child) {
        kprintf("[KTEST FAIL] scheduler_test: process_get failed for pid %u\n", pid);
        return;
    }

    if (child->state != PROCESS_READY) {
        kprintf("[KTEST FAIL] scheduler_test: expected PROCESS_READY, got %u\n", child->state);
        process_reap(child);
        return;
    }

    child->state = PROCESS_ZOMBIE;
    process_reap(child);

    if (child->pid != 0) {
        kprintf("[KTEST FAIL] scheduler_test: process_reap did not release pid %u\n", pid);
        return;
    }

    kprintf("[KTEST] scheduler_test passed\n");
}
