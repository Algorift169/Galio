/* mem_test.c - Memory stabilization tests */
#include "pmem.h"
#include "kprintf.h"

#define FRAME_SIZE 4096
#define MAX_FRAMES 32  /* Limit to avoid excessive logging */

static u32 allocated_frames[MAX_FRAMES];
static u32 num_allocated = 0;
static u32 tests_passed = 0;
static u32 tests_failed = 0;

#define ASSERT(condition, msg) do { \
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
        ASSERT(aligned, "Frame not page-aligned");
    }

    kprintf("TEST 1 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
}

static void test_reserved_ranges(void) {
    kprintf("\nTEST 2: Reserved Range Check\n");
    tests_passed = 0;
    tests_failed = 0;

    /* Check that Frame 0 was never allocated */
    for (u32 i = 0; i < num_allocated; i++) {
        u32 is_frame_zero = (allocated_frames[i] == 0x0);
        ASSERT(!is_frame_zero, "Allocated frame 0 (NULL)");

        /* Kernel range: 0x100000-0x400000 */
        u32 in_kernel_range = (allocated_frames[i] >= 0x100000 && allocated_frames[i] < 0x400000);
        ASSERT(!in_kernel_range, "Allocated in kernel range (0x100000-0x400000)");
    }

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

    /* Verify invariant */
    u32 accounting_correct = (free_before + used_before == total);
    ASSERT(accounting_correct, "Memory accounting mismatch");

    /* Allocate until exhaustion */
    u32 alloc_count = 0;
    while (alloc_count < 256) {  /* Safety limit */
        u32 addr = pmem_alloc(1);
        if (addr == 0) {
            break;
        }
        alloc_count++;

        if (alloc_count <= 5 || alloc_count % 64 == 0) {
            kprintf("  Allocated frame %u: addr=%x\n", alloc_count - 1, addr);
        }
    }

    u32 free_after = pmem_get_free();
    u32 used_after = pmem_get_used();

    kprintf("  Allocated %u frames before OOM\n", alloc_count);
    kprintf("  Memory after: free=%u KB, used=%u KB\n", free_after / 1024, used_after / 1024);

    /* Free all allocated test frames */
    for (u32 i = 0; i < num_allocated; i++) {
        pmem_free(allocated_frames[i], 1);
    }

    u32 free_recovered = pmem_get_free();
    kprintf("  Memory recovered after free: free=%u KB\n", free_recovered / 1024);

    /* Check that we recovered memory for the freed frames */
    u32 expected_free = free_before + (num_allocated * FRAME_SIZE / 1024);
    ASSERT(free_recovered >= expected_free - 4, "Did not recover free memory");  /* Allow small margin */

    kprintf("TEST 3 RESULT: %u passed, %u failed\n", tests_passed, tests_failed);
    num_allocated = 0;
}

static void test_realloc_pattern(void) {
    kprintf("\nTEST 4: Allocate/Free/Realloc Pattern\n");
    tests_passed = 0;
    tests_failed = 0;

    u32 free_start = pmem_get_free();

    /* Phase 1: Allocate 10 frames */
    u32 ptrs[10];
    for (int i = 0; i < 10; i++) {
        u32 addr = pmem_alloc(1);
        ptrs[i] = addr;
        kprintf("  Phase 1: Allocated frame %u at %x\n", i, addr);
        ASSERT(addr != 0, "Allocation failed");
    }

    u32 free_after_alloc = pmem_get_free();

    /* Phase 2: Free odd-numbered frames */
    for (int i = 1; i < 10; i += 2) {
        pmem_free(ptrs[i], 1);
        kprintf("  Phase 2: Freed frame %u at %x\n", i, ptrs[i]);
    }

    u32 free_after_partial_free = pmem_get_free();

    /* Phase 3: Reallocate 5 frames (should reuse freed ones) */
    u32 realloc_ptrs[5];
    for (int i = 0; i < 5; i++) {
        u32 addr = pmem_alloc(1);
        realloc_ptrs[i] = addr;
        kprintf("  Phase 3: Reallocated frame %u at %x\n", i, addr);
        ASSERT(addr != 0, "Reallocation failed");
    }

    u32 free_final = pmem_get_free();

    kprintf("  Free memory: start=%u KB, after alloc=%u KB, after free=%u KB, final=%u KB\n",
            free_start / 1024, free_after_alloc / 1024, free_after_partial_free / 1024, free_final / 1024);

    /* Clean up */
    for (int i = 0; i < 10; i++) {
        pmem_free(ptrs[i], 1);
    }
    for (int i = 0; i < 5; i++) {
        pmem_free(realloc_ptrs[i], 1);
    }

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