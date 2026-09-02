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

/* security_test.c - Kernel security hardening tests (combined)
 * These tests exercise PID allocation, user pointer validation and basic
 * paging behaviors. They are small and safe to run during boot tests.
 */
#include "kprintf.h"
#include "process.h"
#include "syscall.h"
#include "paging.h"
#include "pmem.h"

void security_test(void) {
    kprintf("[KTEST] Running security tests...\n");

    /* PID allocator uniqueness (allocate few PIDs and ensure no duplicates) */
    u32 seen[8] = {0};
    for (u32 i = 0; i < 8; i++) {
        u32 pid = process_allocate_pid();
        kprintf("[KTEST] Allocated PID %u\n", pid);
        for (u32 j = 0; j < i; j++) {
            if (seen[j] == pid) {
                kprintf("[KTEST][FAIL] PID reused: %u\n", pid);
                return;
            }
        }
        seen[i] = pid;
    }

    /* Validate user pointer rejection for kernel address */
    if (validate_user_ptr((void *)KERNEL_BASE, 0)) {
        kprintf("[KTEST][FAIL] Kernel address accepted as user pointer\n");
        return;
    }

    /* Basic paging checks: create a temporary page directory and map a user page */
    page_directory_t *pd = paging_create_directory();
    if (!pd) {
        kprintf("[KTEST FAIL] security_test: paging_create_directory failed\n");
        return;
    }

    u32 phys = pmem_alloc(1);
    if (!phys) {
        kprintf("[KTEST FAIL] security_test: pmem_alloc failed\n");
        pmem_free((u32)pd->directory, 1);
        return;
    }

    const u32 user_page = 0x40000000u;
    paging_map(pd, user_page, phys, PAGE_PRESENT | PAGE_USER);

    if (!paging_validate_user_range(pd, user_page, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: readable user page rejected\n");
    }
    if (paging_validate_user_range(pd, user_page, 16, 1)) {
        kprintf("[KTEST FAIL] security_test: read-only user page accepted for write\n");
    }
    if (paging_validate_user_range(pd, KERNEL_SPACE_START, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: kernel address accepted as user range\n");
    }
    if (paging_validate_user_range(pd, 0xFFFFF000u, 0x2000u, 0)) {
        kprintf("[KTEST FAIL] security_test: overflowing user range accepted\n");
    }
    if (paging_validate_user_range(pd, user_page + PAGE_SIZE, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: unmapped user page accepted\n");
    }

    paging_unmap(pd, user_page);
    if (pd->tables[(user_page >> 22) & 0x3FFu]) {
        pmem_free((u32)pd->tables[(user_page >> 22) & 0x3FFu], 1);
    }
    pmem_free((u32)pd->directory, 1);
    pmem_free(phys, 1);

    kprintf("[KTEST] Security tests passed (combined checks)\n");
}
