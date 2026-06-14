/* scheduler.c - Preemptive scheduler using PIT timer */

#include "process.h"
#include "pit.h"
#include "kprintf.h"

/* Scheduler tick handler - called by PIT and performs preemption */
void scheduler_tick(registers_t *regs) {
    process_t *current = process_current();
    if (current) {
        current->ticks++;

        if (current->time_slice > 0) {
            current->time_slice--;
        }

        if (current->time_slice == 0 && current->state == PROCESS_RUNNING) {
            current->time_slice = PROCESS_TIME_SLICE;
            process_preempt(regs);
        }
    }
}

/* Initialize scheduler - preemptive round-robin mode */
void scheduler_init(void) {
    pit_install_callback(scheduler_tick);
    kprintf("Scheduler initialized (preemptive mode)\n");
    kprintf("  - Timer running at 100Hz with time slice %u\n", PROCESS_TIME_SLICE);
    kprintf("  - Context switches on yield() or when a process exhausts its slice\n");
}

/* CPU statistics - calculate from all processes via process_t accessors */
u32 process_get_total_ticks(void) {
    u32 total = 0;
    u32 count = process_count_active();
    
    for (u32 i = 1; i < count; i++) {
        process_t *proc = process_get(i);
        if (proc && proc->pid != 0) {
            total += proc->ticks;
        }
    }
    return total;
}

u32 process_get_idle_ticks(void) {
    process_t *idle = process_get(0);
    if (idle && idle->pid == 0) {
        return idle->ticks;
    }
    return 0;
}

u8 process_get_cpu_usage(void) {
    u32 total = process_get_total_ticks();
    if (total == 0) return 0;
    
    u32 idle = process_get_idle_ticks();
    u32 used = total - idle;
    
    return (u8)((used * 100) / total);
}
