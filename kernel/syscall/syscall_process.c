/* Process-related syscall implementations. */
#include "syscall.h"
#include "time/galio_time.h"

void syscall_sleep(u32 ms) {
    galio_msleep(ms);
}

i32 syscall_wait_time(u64 seconds) {
    if (seconds > 0xFFFFFFFFu / 1000u) return -22;
    galio_msleep((u32)(seconds * 1000u));
    return 0;
}