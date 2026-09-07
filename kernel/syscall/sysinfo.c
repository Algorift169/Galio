#include "syscall.h"
#include "sysinfo.h"
#include "heap.h"
#include "kernel_time.h"
#include "pmem.h"
#include "process.h"
#include "string.h"

i32 syscall_sysinfo(void *info) {
    galio_sysinfo_t snapshot;

    if (!validate_user_buffer(info, sizeof(snapshot), 1)) {
        return -14;
    }

    snapshot.uptime = kernel_time_get_uptime_seconds();
    snapshot.totalram = pmem_get_total();
    snapshot.freeram = pmem_get_free();
    snapshot.sharedram = 0;
    snapshot.bufferram = heap_get_used_memory();
    snapshot.totalhigh = heap_get_total_memory();
    snapshot.freehigh = snapshot.totalhigh > snapshot.bufferram ?
        snapshot.totalhigh - snapshot.bufferram : 0;
    snapshot.procs = process_count_active();
    snapshot.cpus = 1;

    memcpy(info, &snapshot, sizeof(snapshot));
    return 0;
}