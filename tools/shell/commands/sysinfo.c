#include "shell_sysinfo.h"
#include "syscall.h"
#include "kprintf.h"
#include "user_syscall.h"

u8 shell_sysinfo_command(const char *args, const char *current_dir) {
    galio_sysinfo_t info;
    int result;
    (void)args;
    (void)current_dir;

    result = sys_sysinfo(&info);
    if (result < 0) {
        kprintf("sysinfo: failed (%d)\n", result);
        return 0;
    }

    kprintf("uptime: %u s | processes: %u | cpus: %u\n",
             (u32)info.uptime, info.procs, info.cpus);
    kprintf("physical memory: %uK total | %uK free\n",
             (u32)(info.totalram / 1024), (u32)(info.freeram / 1024));
    kprintf("heap: %uK used | %uK free\n",
             (u32)(info.bufferram / 1024), (u32)(info.freehigh / 1024));
    return 1;
}