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

#include "kprintf.h"
#include "process.h"

#define CPU_SCHED_TEST_JOBS 4

static u32 cpu_scheduler_sequence[CPU_SCHED_TEST_JOBS];
static u32 cpu_scheduler_index = 0;

static void cpu_scheduler_job_1(void) {
    kprintf("[KTEST] cpu_scheduler_job_1 running\n");
    cpu_scheduler_sequence[cpu_scheduler_index++] = 1;
    process_exit(0);
}

static void cpu_scheduler_job_2(void) {
    cpu_scheduler_sequence[cpu_scheduler_index++] = 2;
    process_exit(0);
}

static void cpu_scheduler_job_3(void) {
    cpu_scheduler_sequence[cpu_scheduler_index++] = 3;
    process_exit(0);
}

static void cpu_scheduler_job_4(void) {
    cpu_scheduler_sequence[cpu_scheduler_index++] = 4;
    process_exit(0);
}

void cpu_scheduler_test(void) {
    kprintf("[KTEST] cpu_scheduler_test starting\n");
    kprintf("[KTEST] About to create first process\n");
    
    u32 pid1 = process_create(cpu_scheduler_job_1, 6);
    kprintf("[KTEST] Created PID %u\n", pid1);
    
    if (!pid1) {
        kprintf("[KTEST FAIL] process_create failed\n");
        return;
    }
    
    kprintf("[KTEST] Now attempting waitpid\n");
    i32 child_pid = process_waitpid(-1);
    kprintf("[KTEST] Got child PID %d\n", child_pid);
    
    kprintf("[KTEST] cpu_scheduler_test completed\n");
}
