#ifndef GALIO_SYSCALL_CATALOG_H
#define GALIO_SYSCALL_CATALOG_H

#include "syscall.h"

typedef enum {
    SYSCALL_STATUS_IMPLEMENTED,
    SYSCALL_STATUS_ENOSYS,
    SYSCALL_STATUS_UNDISPATCHED
} syscall_status_t;

typedef struct {
    const char *name;
    u32 number;
    const char *arguments;
    syscall_status_t status;
} syscall_catalog_entry_t;

u32 syscall_catalog_count(void);
const syscall_catalog_entry_t *syscall_catalog_at(u32 index);
const syscall_catalog_entry_t *syscall_catalog_find(const char *name);
const syscall_catalog_entry_t *syscall_catalog_find_number(u32 number);
const char *syscall_status_name(syscall_status_t status);

#endif