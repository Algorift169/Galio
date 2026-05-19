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