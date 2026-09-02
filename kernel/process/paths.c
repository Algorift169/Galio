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

/* paths.c - Current working directory helpers for processes */
#include "process.h"
#include "vfs.h"
#include "path.h"
#include "string.h"

char *process_resolve_path(const char *cwd, const char *path, char *output, u32 output_size) {
    if (!output || output_size == 0) {
        return NULL;
    }

    return path_resolve(cwd, path, output, output_size);
}

u32 process_chdir(const char *path) {
    process_t *proc = process_current();
    if (!proc || !path) {
        return (u32)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, path, resolved, PROCESS_PATH_MAX)) {
        return (u32)-1;
    }

    if (!vfs_is_dir(resolved)) {
        return (u32)-1;
    }

    strncpy(proc->cwd, resolved, PROCESS_PATH_MAX - 1);
    proc->cwd[PROCESS_PATH_MAX - 1] = 0;
    return 0;
}

u32 process_getcwd(char *buffer, u32 size) {
    process_t *proc = process_current();
    if (!proc || !buffer || size == 0) {
        return 0;
    }
    u32 len = strlen(proc->cwd);
    if (len + 1 > size) {
        len = size - 1;
    }
    memcpy(buffer, proc->cwd, len);
    buffer[len] = 0;
    return len;
}
