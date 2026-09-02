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

#include "clean.h"
#include "kprintf.h"
#include "string.h"
#include <string.h>
#include "path.h"
#include "vfs.h"

#define RECYCLE_BIN_DIR "./usr/home/desktop/recycle"

static const char *skip_spaces(const char *str) {
    while (*str == ' ') str++;
    return str;
}

static void build_fullpath(const char *args, const char *current_dir, char *out_path) {
    const char *src = skip_spaces(args);
    if (*src == '\0') {
        out_path[0] = '\0';
        return;
    }

    path_resolve(current_dir, src, out_path, VFS_MAX_PATH);
}

u8 shell_clean_command(const char *args, const char *current_dir) {
    if (!args || *skip_spaces(args) == '\0') {
        kprintf("[CLEAN] Usage: clean <dirname>\n");
        return 0;
    }

    char fullpath[VFS_MAX_PATH];
    build_fullpath(args, current_dir, fullpath);

    if (fullpath[0] == '\0') {
        kprintf("[CLEAN] Invalid directory path\n");
        return 0;
    }

    char normalized[VFS_MAX_PATH];
    if (path_normalize(fullpath, normalized, sizeof(normalized))) {
        if (strcmp(normalized, ".") == 0) {
            kprintf("[CLEAN] Permission denied: use 'rex clean %s' to clean root-level directories\n", args);
            return 0;
        }
        char parent[VFS_MAX_PATH];
        path_parent(normalized, parent, sizeof(parent));
        if (strcmp(parent, ".") == 0) {
            kprintf("[CLEAN] Permission denied: use 'rex clean %s' to clean root-level directories\n", args);
            return 0;
        }
    }

    // Allow "rbin" as alias for recycle bin
    char target[VFS_MAX_PATH];
    strncpy(target, skip_spaces(args), VFS_MAX_PATH - 1);
    target[VFS_MAX_PATH - 1] = 0;
    int target_len = strlen(target);
    while (target_len > 0 && target[target_len - 1] == ' ') {
        target[--target_len] = '\0';
    }
    if (strcmp(target, "rbin") == 0) {
        strncpy(fullpath, RECYCLE_BIN_DIR, VFS_MAX_PATH - 1);
        fullpath[VFS_MAX_PATH - 1] = 0;
    }

    if (!vfs_is_dir(fullpath)) {
        kprintf("[CLEAN] Directory not found: %s\n", fullpath);
        return 0;
    }

    if (!vfs_remove_dir_contents(fullpath)) {
        kprintf("[CLEAN] Failed to clean directory: %s\n", fullpath);
        return 0;
    }

    kprintf("[CLEAN] Directory cleared: %s\n", fullpath);
    vfs_fsync();  /* Ensure cleanup is written to disk */
    return 1;
}