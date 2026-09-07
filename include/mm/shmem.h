#ifndef GALIO_SHMEM_H
#define GALIO_SHMEM_H

#include "common.h"
#include "process/process.h"

#define GALIO_SHM_MAX_SEGMENTS 16
#define GALIO_SHM_MAX_PAGES 256
#define GALIO_SHM_CREAT 0x200
#define GALIO_SHM_EXCL  0x400
#define GALIO_SHM_RMID  0
#define GALIO_SHM_STAT  1

void shmem_init(void);
i32 shmem_get(u32 key, u32 size, i32 flags);
uintptr_t shmem_attach(process_t *proc, i32 shmid, uintptr_t requested_addr);
i32 shmem_detach(process_t *proc, uintptr_t address);
i32 shmem_control(process_t *proc, i32 shmid, i32 command, void *buffer);
void shmem_detach_process(u32 pid);

#endif /* GALIO_SHMEM_H */