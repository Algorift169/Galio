#include "syscall.h"
#include "vfs.h"

i32 syscall_fsync(u32 fd) {
    (void)fd;
    return vfs_fsync() ? 0 : -5;
}