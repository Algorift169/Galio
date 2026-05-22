/* signal_test.c - Kernel signal tests */
#include "kprintf.h"
#include "process.h"
#include "signals.h"

static void signal_dummy(void) {
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void signal_test(void) {
    kprintf("[KTEST] signal_test\n");

    u32 pid = process_create(signal_dummy, 1);
    if (!pid) {
        kprintf("[KTEST FAIL] signal_test: process_create failed\n");
        return;
    }

    process_t *target = process_get_any(pid);
    if (!target) {
        kprintf("[KTEST FAIL] signal_test: process_get_any failed\n");
        return;
    }

    if (!process_send_signal(pid, SIGKILL)) {
        kprintf("[KTEST FAIL] signal_test: process_send_signal failed\n");
        process_reap(target);
        return;
    }

    process_handle_pending_signals(target);

    if (target->state != PROCESS_ZOMBIE) {
        kprintf("[KTEST FAIL] signal_test: expected PROCESS_ZOMBIE, got %u\n", target->state);
        process_reap(target);
        return;
    }

    if (target->exit_code != SIGKILL) {
        kprintf("[KTEST FAIL] signal_test: expected exit_code SIGKILL (%u), got %u\n", SIGKILL, target->exit_code);
        process_reap(target);
        return;
    }

    process_reap(target);
    if (target->pid != 0) {
        kprintf("[KTEST FAIL] signal_test: process_reap failed to clear pid\n");
        return;
    }

    kprintf("[KTEST] signal_test passed\n");
}
