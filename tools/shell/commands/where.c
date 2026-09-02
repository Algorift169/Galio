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

#include "where.h"
#include "kprintf.h"
#include "path.h"
#include "string.h"
#include "vfs.h"
#include "auth.h"

static const char *skip_spaces(const char *text) {
    while (text && (*text == ' ' || *text == '\t')) text++;
    return text;
}

static u8 token_is_basename(const char *token) {
    while (*token) {
        if (*token == '/') return 0;
        token++;
    }
    return 1;
}

static const char *entry_basename(const char *path) {
    const char *base = path;
    while (*path) {
        if (*path == '/') base = path + 1;
        path++;
    }
    return base;
}

static u8 can_search_root_tree(const char *current_dir) {
    char normalized[VFS_MAX_PATH];
    if (auth_is_authorized()) return 1;
    if (!current_dir || !path_normalize(current_dir, normalized, sizeof(normalized))) {
        return 0;
    }
    return strcmp(normalized, ".") == 0;
}

static u8 path_is_in_current_tree(const char *path, const char *current_dir) {
    char normalized_path[VFS_MAX_PATH];
    char normalized_dir[VFS_MAX_PATH];

    if (!path || !current_dir ||
        !path_normalize(path, normalized_path, sizeof(normalized_path)) ||
        !path_normalize(current_dir, normalized_dir, sizeof(normalized_dir))) {
        return 0;
    }
    if (strcmp(normalized_dir, ".") == 0) return 1;
    u32 dir_len = strlen(normalized_dir);
    return strcmp(normalized_path, normalized_dir) == 0 ||
           (strncmp(normalized_path, normalized_dir, dir_len) == 0 &&
            normalized_path[dir_len] == '/');
}

u8 shell_where_command(const char *args, const char *current_dir) {
    char token[VFS_MAX_PATH];
    const char *src = skip_spaces(args);
    u32 length = 0;

    if (!src || *src == 0) {
        kprintf("[WHERE] Usage: where <file-or-directory>\n");
        return 0;
    }

    while (src[length] && src[length] != ' ' && src[length] != '\t' &&
           length < VFS_MAX_PATH - 1) {
        token[length] = src[length];
        length++;
    }
    token[length] = 0;

    if (src[length] != 0) {
        const char *extra = skip_spaces(src + length);
        if (*extra != 0) {
            kprintf("[WHERE] Usage: where <file-or-directory>\n");
            return 0;
        }
    }

    char resolved[VFS_MAX_PATH];
    path_resolve(current_dir ? current_dir : ".", token, resolved, sizeof(resolved));
    vfs_entry_t *exact = vfs_find(resolved);
    if (exact && (can_search_root_tree(current_dir) ||
                  path_is_in_current_tree(exact->path, current_dir))) {
        kprintf("%s\n", exact->path);
        return 1;
    }

    if (!token_is_basename(token) || !vfs_root) {
        kprintf("[WHERE] Not found: %s\n", token);
        return 0;
    }

    u8 found = 0;
    for (u32 i = 0; i < vfs_root->entry_count; i++) {
        const char *path = vfs_root->entries[i].path;
        if (!can_search_root_tree(current_dir) &&
            !path_is_in_current_tree(path, current_dir)) {
            continue;
        }
        if (strcmp(entry_basename(path), token) == 0) {
            kprintf("%s\n", path);
            found = 1;
        }
    }

    if (!found) {
        kprintf("[WHERE] Not found: %s\n", token);
    }
    return found;
}
