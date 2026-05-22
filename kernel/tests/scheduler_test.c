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
