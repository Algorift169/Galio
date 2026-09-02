/*
 * Galio Kernel
 *
 * Copyright (C) 2026 Israfil [Your Legal Name]
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
