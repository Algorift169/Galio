#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include "common.h"
#include "process.h"

#define PROCESS_INFO_NAME_MAX PROCESS_PATH_MAX

typedef enum {
    PROCESS_INFO_USER,
    PROCESS_INFO_KERNEL
} process_info_type_t;

typedef struct {
    u32 pid;
    u32 parent_pid;
    process_state_t state;
    process_info_type_t type;
    u32 priority;
    u64 runtime_ticks;
    u32 memory_bytes;
    char path[PROCESS_INFO_NAME_MAX];
} process_info_t;

/* Copies active process state while the process table is locked. */
u32 process_snapshot(process_info_t *entries, u32 capacity);

#endif /* PROCESS_INFO_H */
