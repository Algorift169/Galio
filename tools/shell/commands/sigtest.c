/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

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
