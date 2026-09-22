#include "kernel/workqueue.h"
#include "arch/x86/cpu.h"
#include "drivers/pit.h"

#define WORKQUEUE_MAX_ITEMS 64u

typedef struct {
    workqueue_fn function;
    void *arg;
} work_item_t;

static work_item_t work_items[WORKQUEUE_MAX_ITEMS];
static volatile u32 work_head;
static volatile u32 work_tail;

static void workqueue_tick(registers_t *regs) {
    (void)regs;
    workqueue_run();
}

void workqueue_init(void) {
    u64 flags = irq_save();
    work_head = 0;
    work_tail = 0;
    for (u32 i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        work_items[i].function = NULL;
        work_items[i].arg = NULL;
    }
    irq_restore(flags);
    pit_install_callback(workqueue_tick);
}

int workqueue_schedule(workqueue_fn function, void *arg) {
    u64 flags;
    u32 next;

    if (!function) return -1;
    flags = irq_save();
    next = (work_tail + 1u) % WORKQUEUE_MAX_ITEMS;
    if (next == work_head) {
        irq_restore(flags);
        return -1;
    }
    work_items[work_tail].function = function;
    work_items[work_tail].arg = arg;
    work_tail = next;
    irq_restore(flags);
    return 0;
}

static u32 workqueue_filter(work_fn_t function, void *arg) {
    work_item_t pending[WORKQUEUE_MAX_ITEMS];
    work_item_t kept[WORKQUEUE_MAX_ITEMS];
    u32 pending_count = 0u;
    u32 kept_count = 0u;
    u32 removed = 0u;

    while (work_head != work_tail) {
        pending[pending_count++] = work_items[work_head];
        work_items[work_head].function = NULL;
        work_items[work_head].arg = NULL;
        work_head = (work_head + 1u) % WORKQUEUE_MAX_ITEMS;
    }
    work_head = 0u;
    work_tail = 0u;
    for (u32 i = 0; i < pending_count; i++) {
        work_item_t item = pending[i];
        if (item.function == function && item.arg == arg) {
            removed++;
        } else if (kept_count < WORKQUEUE_MAX_ITEMS) {
            kept[kept_count++] = item;
        }
    }

    work_tail = kept_count;
    for (u32 i = 0; i < kept_count; i++) work_items[i] = kept[i];
    return removed;
}

int workqueue_cancel(work_fn_t function, void *arg) {
    u64 flags;
    u32 removed;
    if (!function) return -1;
    flags = irq_save();
    removed = workqueue_filter(function, arg);
    irq_restore(flags);
    return (int)removed;
}

void workqueue_flush_fn(work_fn_t function, void *arg) {
    work_item_t pending[WORKQUEUE_MAX_ITEMS];
    work_item_t kept[WORKQUEUE_MAX_ITEMS];
    work_item_t matching[WORKQUEUE_MAX_ITEMS];
    u32 pending_count = 0u;
    u32 kept_count = 0u;
    u32 matching_count = 0u;
    u64 flags;
    if (!function) return;
    flags = irq_save();
    while (work_head != work_tail) {
        pending[pending_count++] = work_items[work_head];
        work_items[work_head].function = NULL;
        work_items[work_head].arg = NULL;
        work_head = (work_head + 1u) % WORKQUEUE_MAX_ITEMS;
    }
    work_head = 0u;
    work_tail = 0u;
    for (u32 i = 0; i < pending_count; i++) {
        work_item_t item = pending[i];
        if (item.function == function && item.arg == arg) {
            if (matching_count < WORKQUEUE_MAX_ITEMS) {
                matching[matching_count++] = item;
            }
        } else if (kept_count < WORKQUEUE_MAX_ITEMS) {
            kept[kept_count++] = item;
        }
    }
    work_tail = kept_count;
    for (u32 i = 0; i < kept_count; i++) work_items[i] = kept[i];
    irq_restore(flags);
    for (u32 i = 0; i < matching_count; i++) matching[i].function(matching[i].arg);
}

void workqueue_run(void) {
    for (;;) {
        workqueue_fn function;
        void *arg;
        u64 flags = irq_save();

        if (work_head == work_tail) {
            irq_restore(flags);
            return;
        }
        function = work_items[work_head].function;
        arg = work_items[work_head].arg;
        work_items[work_head].function = NULL;
        work_items[work_head].arg = NULL;
        work_head = (work_head + 1u) % WORKQUEUE_MAX_ITEMS;
        irq_restore(flags);
        function(arg);
    }
}