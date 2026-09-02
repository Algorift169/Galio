/* Process-related syscall implementations. */
#include "syscall.h"
#include "time/galio_time.h"

void syscall_sleep(u32 ms) {
    galio_msleep(ms);
}