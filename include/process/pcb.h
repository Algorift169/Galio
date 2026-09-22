#ifndef GALIO_PROCESS_PCB_H
#define GALIO_PROCESS_PCB_H

#include "process/process.h"

typedef struct {
    uintptr_t heap_start;
    uintptr_t brk;
    uintptr_t heap_limit;
    uintptr_t stack_top;
    uintptr_t stack_size;
    u32 memory_bytes;
    u32 mmap_count;
    const mmap_region_t *mmap_regions;
} pcb_memory_limits_t;

typedef struct {
    u64 runtime_ticks;
    u32 ticks;
    u32 time_slice;
    u32 priority;
    u8 accounting_idle;
} pcb_accounting_t;

typedef struct {
    i32 waiting_for_pid;
    u32 pending_signals;
    u32 exit_code;
    process_state_t state;
} pcb_io_status_t;

void pcb_initialize(pcb_t *pcb);
process_state_t pcb_get_state(const pcb_t *pcb);
void pcb_set_state(pcb_t *pcb, process_state_t state);

u32 pcb_get_process_number(const pcb_t *pcb);
void pcb_set_process_number(pcb_t *pcb, u32 pid);

uintptr_t pcb_get_program_counter(const pcb_t *pcb);
void pcb_set_program_counter(pcb_t *pcb, uintptr_t program_counter);

const register_state_t *pcb_get_registers(const pcb_t *pcb);
register_state_t *pcb_get_registers_mutable(pcb_t *pcb);

int pcb_get_memory_limits(const pcb_t *pcb, pcb_memory_limits_t *limits);

u32 pcb_get_open_file(const pcb_t *pcb, u32 index);
int pcb_set_open_file(pcb_t *pcb, u32 index, u32 handle);
u32 pcb_get_open_file_count(const pcb_t *pcb);

void pcb_get_accounting(const pcb_t *pcb, pcb_accounting_t *accounting);
void pcb_accounting_tick(pcb_t *pcb);

void pcb_get_io_status(const pcb_t *pcb, pcb_io_status_t *status);

#endif /* GALIO_PROCESS_PCB_H */