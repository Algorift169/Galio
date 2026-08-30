#include "shell.h"
#include "kprintf.h"
#include "user_syscall.h"
#include <string.h>

u8 shell_sigtest_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!args || *args == '\0') {
        kprintf("sigtest: test signal syscalls\n");
        kprintf("usage: sigtest [list|action|mask|return]\n");
        return 0;
    }
    
    if (strcmp(args, "list") == 0) {
        kprintf("Available signal syscalls:\n");
        kprintf("  rt_sigaction()   - Install signal handler\n");
        kprintf("  rt_sigprocmask() - Manipulate signal mask\n");
        kprintf("  rt_sigreturn()   - Return from signal handler\n");
        kprintf("  pause()          - Wait for signal\n");
        return 1;
    }
    
    if (strcmp(args, "action") == 0) {
        kprintf("rt_sigaction: would install signal handler\n");
        kprintf("  (Currently returns ENOSYS)\n");
        return 1;
    }
    
    if (strcmp(args, "mask") == 0) {
        kprintf("rt_sigprocmask: would manipulate signal mask\n");
        kprintf("  (Currently returns ENOSYS)\n");
        return 1;
    }
    
    if (strcmp(args, "return") == 0) {
        kprintf("rt_sigreturn: signal handler return\n");
        kprintf("  (Used internally, not directly callable)\n");
        return 1;
    }
    
    kprintf("sigtest: unknown subcommand '%s'\n", args);
    return 0;
}
