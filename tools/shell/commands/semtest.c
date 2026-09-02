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

u8 shell_semtest_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!args || *args == '\0') {
        kprintf("semtest: test semaphore syscalls\n");
        kprintf("usage: semtest [get|op|ctl|list]\n");
        return 0;
    }
    
    if (strcmp(args, "get") == 0) {
        /* Test semget - get/create semaphore set */
        int semid = sys_semget(1234, 1, 0666);
        kprintf("semget(key=1234, nsems=1, flags=0666) = %d\n", semid);
        return 1;
    }
    
    if (strcmp(args, "op") == 0) {
        /* Test semop - semaphore operation */
        kprintf("semop: would perform semaphore operation\n");
        kprintf("  (requires valid semaphore ID)\n");
        return 1;
    }
    
    if (strcmp(args, "ctl") == 0) {
        /* Test semctl - semaphore control */
        kprintf("semctl: would perform semaphore control\n");
        kprintf("  (requires valid semaphore ID)\n");
        return 1;
    }
    
    if (strcmp(args, "list") == 0) {
        kprintf("Available semaphore syscalls:\n");
        kprintf("  semget()   - Create/get semaphore set\n");
        kprintf("  semop()    - Perform semaphore operation\n");
        kprintf("  semctl()   - Semaphore control operations\n");
        return 1;
    }
    
    kprintf("semtest: unknown subcommand '%s'\n", args);
    return 0;
}
