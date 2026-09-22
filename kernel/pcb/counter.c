#include "process/pcb.h"

uintptr_t pcb_get_program_counter(const pcb_t *pcb) {
    return pcb ? pcb->regs.rip : 0u;
}

void pcb_set_program_counter(pcb_t *pcb, uintptr_t program_counter) {
    if (pcb) {
        pcb->regs.rip = program_counter;
        pcb->regs.eip = program_counter;
    }
}

void pcb_get_accounting(const pcb_t *pcb, pcb_accounting_t *accounting) {
    if (!pcb || !accounting) return;
    accounting->runtime_ticks = pcb->runtime_ticks;
    accounting->ticks = pcb->ticks;
    accounting->time_slice = pcb->time_slice;
    accounting->priority = pcb->priority;
    accounting->accounting_idle = pcb->accounting_idle;
}

void pcb_accounting_tick(pcb_t *pcb) {
    if (!pcb) return;
    pcb->ticks++;
    pcb->runtime_ticks++;
    if (pcb->time_slice > 0u) pcb->time_slice--;
}