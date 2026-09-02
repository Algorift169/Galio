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

#include "access.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"

u8 shell_access_command(const char *args, const char *current_dir) {
    if (!args || *args == '\0') {
        kprintf("Usage: access <path> [mode]\n");
        return 0;
    }

    char path[512];
    if (!path_resolve(current_dir, args, path, sizeof(path))) {
        kprintf("access: invalid path\n");
        return 0;
    }

    const char *mode_str = "r";
    if (strchr(args, ' ') != NULL) {
        char tmp[256];
        strncpy(tmp, args, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *space = strchr(tmp, ' ');
        if (space) {
            *space = '\0';
            mode_str = space + 1;
        }
    }

    int mode = 0;
    if (strcmp(mode_str, "r") == 0) mode = 4;
    else if (strcmp(mode_str, "w") == 0) mode = 2;
    else if (strcmp(mode_str, "x") == 0) mode = 1;
    else if (strcmp(mode_str, "rw") == 0) mode = 6;
    else if (strcmp(mode_str, "rx") == 0) mode = 5;
    else if (strcmp(mode_str, "wx") == 0) mode = 3;
    else if (strcmp(mode_str, "all") == 0) mode = 7;
    else {
        kprintf("access: unsupported mode '%s'\n", mode_str);
        return 0;
    }

    if (vfs_find(path) != NULL) {
        kprintf("access: %s -> ok (%d)\n", path, mode);
        return 1;
    }

    kprintf("access: %s -> denied\n", path);
    return 0;
}
