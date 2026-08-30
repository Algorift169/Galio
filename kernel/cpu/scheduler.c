#include "cpu/scheduler.h"
#include "process.h"
#include "pit.h"
#include "kprintf.h"

void cpu_scheduler_tick(registers_t *regs) {
    process_t *current = process_current();
    if (!current) {
        return;
    }

    current->ticks++;
    if (current->time_slice > 0) {
        current->time_slice--;
    }

    if (regs && (regs->cs & 3) == 3 &&
        current->time_slice == 0 && current->state == PROCESS_RUNNING) {
        current->time_slice = PROCESS_TIME_SLICE;
        process_preempt(regs);
    }
}

void cpu_scheduler_init(void) {
    pit_install_callback(cpu_scheduler_tick);
    kprintf("CPU scheduler initialized (Shortest Job First job scheduler)\n");
    kprintf("  - burst time used to choose shortest ready process\n");
    kprintf("  - time slice = %u\n", PROCESS_TIME_SLICE);
}
