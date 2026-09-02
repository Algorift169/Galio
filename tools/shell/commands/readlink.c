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

#include "readlink.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"

u8 shell_readlink_command(const char *args, const char *current_dir) {
    if (!args || *args == '\0') {
        kprintf("Usage: readlink <path>\n");
        return 0;
    }

    char path[512];
    if (!path_resolve(current_dir, args, path, sizeof(path))) {
        kprintf("readlink: invalid path\n");
        return 0;
    }

    char target[512];
    if (vfs_readlink(path, target, sizeof(target)) != 0) {
        kprintf("%s\n", target);
        return 1;
    }

    kprintf("readlink: %s not a symlink\n", path);
    return 0;
}
