#include "common.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"

u8 shell_syscall_info_command(const char *args, const char *current_dir) {
    (void)current_dir;
    if (!args || *args == 0) {
        kprintf("syscall info: pid uid gid time fork pipe dup mmap brk wait open read close seek stat exec\n");
        return 0;
    }

    kprintf("syscall '%s' is exposed through the INT 0x80 ABI.\n", args);
    return 1;
}
