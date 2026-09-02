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

u8 shell_shmtest_command(const char *args, const char *current_dir) {
    (void)current_dir;
    
    if (!args || *args == '\0') {
        kprintf("shmtest: test shared memory syscalls\n");
        kprintf("usage: shmtest [get|at|dt|ctl|list]\n");
        return 0;
    }
    
    if (strcmp(args, "get") == 0) {
        /* Test shmget - get/create shared memory segment */
        int shmid = sys_shmget(5678, 4096, 0666);
        kprintf("shmget(key=5678, size=4096, flags=0666) = %d\n", shmid);
        return 1;
    }
    
    if (strcmp(args, "at") == 0) {
        /* Test shmat - attach shared memory */
        kprintf("shmat: would attach shared memory segment\n");
        kprintf("  (requires valid shared memory ID)\n");
        return 1;
    }
    
    if (strcmp(args, "dt") == 0) {
        /* Test shmdt - detach shared memory */
        kprintf("shmdt: would detach shared memory segment\n");
        return 1;
    }
    
    if (strcmp(args, "ctl") == 0) {
        /* Test shmctl - shared memory control */
        kprintf("shmctl: would perform shared memory control\n");
        kprintf("  (requires valid shared memory ID)\n");
        return 1;
    }
    
    if (strcmp(args, "list") == 0) {
        kprintf("Available shared memory syscalls:\n");
        kprintf("  shmget()   - Create/get shared memory segment\n");
        kprintf("  shmat()    - Attach shared memory to process\n");
        kprintf("  shmdt()    - Detach shared memory from process\n");
        kprintf("  shmctl()   - Shared memory control operations\n");
        return 1;
    }
    
    kprintf("shmtest: unknown subcommand '%s'\n", args);
    return 0;
}
