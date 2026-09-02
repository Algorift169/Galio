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

/* run_tests.c - Kernel test runner */
#include "kprintf.h"

extern void scheduler_test(void);
extern void cpu_scheduler_test(void);
extern void paging_test(void);
extern void vfs_test(void);
extern void signal_test(void);
extern void heap_test(void);
extern void security_test(void);
extern void cpufreq_test(void);

void run_kernel_tests(void) {
    kprintf("\n========================================\n");
    kprintf("     KERNEL SELF-TEST SUITE\n");
    kprintf("========================================\n");

    scheduler_test();
    cpu_scheduler_test();
    paging_test();
    vfs_test();
    signal_test();
    heap_test();
    security_test();
    cpufreq_test();

    kprintf("========================================\n");
    kprintf("[KTEST] Kernel self-tests completed\n");
    kprintf("========================================\n\n");
}
