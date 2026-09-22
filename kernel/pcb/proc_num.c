#include "process/pcb.h"

u32 pcb_get_process_number(const pcb_t *pcb) {
    return pcb ? pcb->pid : 0u;
}

void pcb_set_process_number(pcb_t *pcb, u32 pid) {
    if (pcb) pcb->pid = pid;
}