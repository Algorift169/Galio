#ifndef GALIO_SYSINFO_H
#define GALIO_SYSINFO_H

#include "common.h"

typedef struct {
    u64 uptime;
    u64 totalram;
    u64 freeram;
    u64 sharedram;
    u64 bufferram;
    u64 totalhigh;
    u64 freehigh;
    u32 procs;
    u32 cpus;
} galio_sysinfo_t;

#endif /* GALIO_SYSINFO_H */