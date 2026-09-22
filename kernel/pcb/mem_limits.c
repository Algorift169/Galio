#include "process/pcb.h"

int pcb_get_memory_limits(const pcb_t *pcb, pcb_memory_limits_t *limits) {
    if (!pcb || !limits) return -1;
    limits->heap_start = pcb->heap_start;
    limits->brk = pcb->brk;
    limits->heap_limit = USER_HEAP_END;
    limits->stack_top = USER_STACK_TOP;
    limits->stack_size = USER_STACK_SIZE;
    limits->memory_bytes = pcb->memory_bytes;
    limits->mmap_count = pcb->mmap_count;
    limits->mmap_regions = pcb->mmap_regions;
    return 0;
}