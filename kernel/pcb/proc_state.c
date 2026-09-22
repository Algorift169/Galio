#include "process/pcb.h"
#include "vfs.h"
#include "string.h"

void pcb_initialize(pcb_t *pcb) {
    if (!pcb) return;
    memset(pcb, 0, sizeof(*pcb));
    pcb->state = PROCESS_ZOMBIE;
    pcb->waiting_for_pid = -1;
    for (u32 index = 0; index < PROCESS_MAX_FDS; index++)
        pcb->fd_table[index] = VFS_INVALID_FD;
}

process_state_t pcb_get_state(const pcb_t *pcb) {
    return pcb ? pcb->state : PROCESS_ZOMBIE;
}

void pcb_set_state(pcb_t *pcb, process_state_t state) {
    if (pcb) pcb->state = state;
}

void pcb_get_io_status(const pcb_t *pcb, pcb_io_status_t *status) {
    if (!pcb || !status) return;
    status->waiting_for_pid = pcb->waiting_for_pid;
    status->pending_signals = pcb->pending_signals;
    status->exit_code = pcb->exit_code;
    status->state = pcb->state;
}