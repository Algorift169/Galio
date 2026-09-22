#include "process/pcb.h"
#include "vfs.h"

u32 pcb_get_open_file(const pcb_t *pcb, u32 index) {
    if (!pcb || index >= PROCESS_MAX_FDS) return VFS_INVALID_FD;
    return pcb->fd_table[index];
}

int pcb_set_open_file(pcb_t *pcb, u32 index, u32 handle) {
    if (!pcb || index >= PROCESS_MAX_FDS) return -1;
    pcb->fd_table[index] = handle;
    return 0;
}

u32 pcb_get_open_file_count(const pcb_t *pcb) {
    u32 count = 0u;
    if (!pcb) return 0u;
    for (u32 index = 0; index < PROCESS_MAX_FDS; index++) {
        if (pcb->fd_table[index] != VFS_INVALID_FD) count++;
    }
    return count;
}