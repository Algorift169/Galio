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

/* mem_test.c - Memory stabilization tests */
#include "pmem.h"
#include "kprintf.h"

#define FRAME_SIZE 4096
#define MAX_FRAMES 32  /* Limit to avoid excessive logging */

static u32 allocated_frames[MAX_FRAMES];
static u32 num_allocated = 0;
static u32 tests_passed = 0;
static u32 tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    if (!(condition)) { \
        kprintf("[TEST FAIL] %s\n", msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while (0)

static void test_alignment(void) {
    kprintf("\nTEST 1: Frame Alignment\n");
    tests_passed = 0;
    tests_failed = 0;

    for (int i = 0; i < 5; i++) {
        u32 addr = pmem_alloc(1);
        if (addr == 0) break;

        allocated_frames[num_allocated++] = addr;

        u32 aligned = (addr & 0xFFF) == 0;
        kprintf("  Allocation %u: addr=%x, aligned=%u\n", i, addr, aligned);
        TEST_ASSERT(aligned, "Frame not page-aligned");
    }

    kprintf("TEST 1 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
}

static void test_reserved_ranges(void) {
    kprintf("\nTEST 2: Reserved Range Check\n");
    tests_passed = 0;
    tests_failed = 0;

    for (u32 i = 0; i < num_allocated; i++) {
        u32 is_frame_zero = (allocated_frames[i] == 0x0);
        TEST_ASSERT(!is_frame_zero, "Allocated frame 0 (NULL)");
        u32 in_kernel_range = (allocated_frames[i] >= 0x100000 && allocated_frames[i] < 0x400000);
        TEST_ASSERT(!in_kernel_range, "Allocated in kernel range (0x100000-0x400000)");
    }

    for (u32 i = 0; i < num_allocated; i++) {
        pmem_free(allocated_frames[i], 1);
    }
    num_allocated = 0;
    kprintf("TEST 2 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
}

static void test_exhaustion(void) {
    kprintf("\nTEST 3: Allocation Exhaustion\n");
    tests_passed = 0;
    tests_failed = 0;

    u32 free_before = pmem_get_free();
    u32 used_before = pmem_get_used();
    u32 total = pmem_get_total();

    kprintf("  Memory status: total=%u KB, used=%u KB, free=%u KB\n",
            total / 1024, used_before / 1024, free_before / 1024);

    u32 accounting_correct = (free_before + used_before == total);
    TEST_ASSERT(accounting_correct, "Memory accounting mismatch");

    u32 exhaustion_frames[256];
    u32 alloc_count = 0;
    while (alloc_count < 256) {
        u32 addr = pmem_alloc(1);
        if (addr == 0) break;
        exhaustion_frames[alloc_count++] = addr;
        if (alloc_count <= 5 || alloc_count % 64 == 0) {
            kprintf("  Allocated frame %u: addr=%x\n", alloc_count - 1, addr);
        }
    }

    u32 free_after = pmem_get_free();
    u32 used_after = pmem_get_used();
    kprintf("  Allocated %u frames before OOM\n", alloc_count);
    kprintf("  Memory after: free=%u KB, used=%u KB\n", free_after / 1024, used_after / 1024);

    for (u32 i = 0; i < alloc_count; i++) {
        pmem_free(exhaustion_frames[i], 1);
    }

    u32 free_recovered = pmem_get_free();
    kprintf("  Memory recovered after free: free=%u KB\n", free_recovered / 1024);
    TEST_ASSERT(free_recovered >= free_before, "Did not recover free memory");

    kprintf("TEST 3 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
    num_allocated = 0;
}

static void test_realloc_pattern(void) {
    kprintf("\nTEST 4: Allocate/Free/Realloc Pattern\n");
    tests_passed = 0;
    tests_failed = 0;

    u32 free_start = pmem_get_free();
    u32 ptrs[10];
    for (int i = 0; i < 10; i++) {
        u32 addr = pmem_alloc(1);
        ptrs[i] = addr;
        kprintf("  Phase 1: Allocated frame %u at %x\n", i, addr);
        TEST_ASSERT(addr != 0, "Allocation failed");
    }

    u32 free_after_alloc = pmem_get_free();
    for (int i = 1; i < 10; i += 2) {
        pmem_free(ptrs[i], 1);
        kprintf("  Phase 2: Freed frame %u at %x\n", i, ptrs[i]);
    }

    u32 free_after_partial_free = pmem_get_free();
    u32 realloc_ptrs[5];
    for (int i = 0; i < 5; i++) {
        u32 addr = pmem_alloc(1);
        realloc_ptrs[i] = addr;
        kprintf("  Phase 3: Reallocated frame %u at %x\n", i, addr);
        TEST_ASSERT(addr != 0, "Reallocation failed");
    }

    u32 free_final = pmem_get_free();
    kprintf("  Free memory: start=%u KB, after alloc=%u KB, after free=%u KB, final=%u KB\n",
            free_start / 1024, free_after_alloc / 1024, free_after_partial_free / 1024, free_final / 1024);

    for (int i = 0; i < 10; i++) pmem_free(ptrs[i], 1);
    for (int i = 0; i < 5; i++) pmem_free(realloc_ptrs[i], 1);
    kprintf("TEST 4 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
}

void mem_test_run(void) {
    u32 total_passed = 0;
    u32 total_failed = 0;

    kprintf("\n========================================\n");
    kprintf("     MEMORY STABILIZATION TESTS\n");
    kprintf("========================================\n");

    test_alignment();
    total_passed += tests_passed;
    total_failed += tests_failed;

    test_reserved_ranges();
    total_passed += tests_passed;
    total_failed += tests_failed;

    test_exhaustion();
    total_passed += tests_passed;
    total_failed += tests_failed;

    test_realloc_pattern();
    total_passed += tests_passed;
    total_failed += tests_failed;

    kprintf("\n========================================\n");
    kprintf("MEMORY_TESTS: %u passed, %u failed\n", total_passed, total_failed);
    if (total_failed == 0) {
        kprintf("STATUS: ALL TESTS PASSED\n");
    } else {
        kprintf("STATUS: SOME TESTS FAILED - investigate\n");
    }
    kprintf("========================================\n\n");
}
