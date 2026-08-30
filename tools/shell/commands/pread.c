#include "shell.h"
#include "kprintf.h"
#include "user_syscall.h"
#include <string.h>


u8 shell_pread_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!args || *args == '\0') {
        kprintf("pread: test pread64/pwrite64 syscalls\n");
        kprintf("usage: pread [info]\n");
        return 0;
    }
    
    if (strcmp(args, "info") == 0) {
        kprintf("Positioned read/write syscalls:\n");
        kprintf("  pread64()   - Read from file at offset\n");
        kprintf("  pwrite64()  - Write to file at offset\n");
        kprintf("  (Don't change file position)\n");
        return 1;
    }
    
    kprintf("pread: unknown subcommand '%s'\n", args);
    return 0;
}
