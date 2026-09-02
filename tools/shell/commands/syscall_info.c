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
