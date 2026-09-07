#ifndef GALIO_SYSCALL_SHMEM_H
#define GALIO_SYSCALL_SHMEM_H

#include "common.h"

typedef struct {
    u32 key;
    u32 size;
    u32 attachments;
    u32 pages;
} galio_shmid_ds_t;

#endif /* GALIO_SYSCALL_SHMEM_H */