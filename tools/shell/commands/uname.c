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

#include "uname.h"
#include "kprintf.h"
#include "string.h"
#include "user_syscall.h"

u8 shell_uname_command(const char *args, const char *current_dir) {
    (void)current_dir;
    struct utsname info;
    if (args && *args != '\0') {
        if (strcmp(args, "-a") == 0 || strcmp(args, "--all") == 0) {
            if (sys_uname(&info) == 0) {
                kprintf("Galio %s %s %s %s %s\n",
                        info.sysname, info.nodename, info.release,
                        info.version, info.machine);
                return 1;
            }
            kprintf("uname: syscall failed\n");
            return 0;
        }
        if (strcmp(args, "-s") == 0) {
            if (sys_uname(&info) == 0) {
                kprintf("%s\n", info.sysname);
                return 1;
            }
        }
    }

    if (sys_uname(&info) == 0) {
        kprintf("%s %s %s %s %s\n",
                info.sysname, info.nodename, info.release,
                info.version, info.machine);
        return 1;
    }

    kprintf("uname: syscall failed\n");
    return 0;
}
