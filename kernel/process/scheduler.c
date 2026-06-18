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
    
    for (u32 pid = 1; pid <= MAX_PROCESSES; pid++) {
        process_t *proc = process_get(pid);
        if (proc) {
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
    extern u32 pit_get_ticks(void);
    static u32 last_update_ticks = 0;
    static u8 cached_usage = 0;
    static u32 last_total = 0;
    static u32 last_idle = 0;

    u32 now = pit_get_ticks();
    if (last_update_ticks == 0 || (now - last_update_ticks) >= 100) {
        u32 total = process_get_total_ticks();
        u32 idle = process_get_idle_ticks();
        
        if (total > 0) {
            u32 delta_total = total - last_total;
            u32 delta_idle = idle - last_idle;
            
            last_total = total;
            last_idle = idle;
            last_update_ticks = now;
            
            if (delta_total > 0) {
                if (delta_idle > delta_total) delta_idle = delta_total;
                cached_usage = (u8)(((delta_total - delta_idle) * 100) / delta_total);
            }
        }
    }
    return cached_usage;
}
