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

#include "file.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"
#include "auth.h"

static const char *skip_spaces(const char *str) {
    while (*str == ' ') str++;
    return str;
}

static void safe_strcat(char *dest, const char *src, u32 max_len) {
    if (!dest || !src || max_len == 0) return;
    u32 dest_len = strlen(dest);
    if (dest_len >= max_len - 1) return;
    u32 copy_len = max_len - dest_len - 1;
    if (copy_len > 0) {
        strncat(dest, src, copy_len);
        dest[max_len - 1] = 0;
    }
}

static u8 has_extension(const char *filename) {
    while (*filename) {
        if (*filename == '.') return 1;
        filename++;
    }
    return 0;
}

static const char *get_basename(const char *path) {
    const char *basename = path;
    while (*path) {
        if (*path == '/') basename = path + 1;
        path++;
    }
    return basename;
}

static void ensure_extension(char *filename, u32 max_len) {
    if (!has_extension(filename)) {
        safe_strcat(filename, ".txt", max_len);
    }
}

static void ensure_extension_on_path(char *path, u32 max_len) {
    const char *basename = get_basename(path);
    if (!has_extension(basename)) {
        safe_strcat(path, ".txt", max_len);
    }
}

static void combine_path(const char *current_dir, const char *relative, char *out_path) {
    path_resolve(current_dir, relative, out_path, VFS_MAX_PATH);
}

static void build_target_path(const char *dir, const char *filename, char *out_path) {
    if (!dir || *dir == '\0') {
        strncpy(out_path, ".", VFS_MAX_PATH - 1);
        out_path[VFS_MAX_PATH - 1] = 0;
    } else {
        strncpy(out_path, dir, VFS_MAX_PATH - 1);
        out_path[VFS_MAX_PATH - 1] = 0;
    }

    u32 out_len = strlen(out_path);
    if (out_len > 0 && out_path[out_len - 1] != '/') {
        safe_strcat(out_path, "/", VFS_MAX_PATH);
    }
    safe_strcat(out_path, filename, VFS_MAX_PATH);
}

static void get_parent_dir(const char *path, char *out_parent) {
    path_parent(path, out_parent, VFS_MAX_PATH);
}

static u8 is_root_child_path(const char *path) {
    char normalized[VFS_MAX_PATH];
    path_normalize(path, normalized, VFS_MAX_PATH);
    if (strcmp(normalized, ".") == 0) return 0;
    char parent[VFS_MAX_PATH];
    path_parent(normalized, parent, VFS_MAX_PATH);
    return strcmp(parent, ".") == 0;
}

static u8 contains_slash(const char *path) {
    while (*path) {
        if (*path == '/') return 1;
        path++;
    }
    return 0;
}

static u8 build_path_and_filename(const char *args, const char *current_dir, char *out_fullpath) {
    char token[VFS_MAX_PATH];
    const char *src = skip_spaces(args);
    if (*src == 0) {
        return 0;
    }

    u32 idx = 0;
    while (*src && *src != ' ' && idx < VFS_MAX_PATH - 1) {
        token[idx++] = *src++;
    }
    token[idx] = 0;

    if (token[0] == 0) {
        return 0;
    }

    if (contains_slash(token)) {
        combine_path(current_dir, token, out_fullpath);
        ensure_extension_on_path(out_fullpath, VFS_MAX_PATH);
        return 1;
    }

    char filename[VFS_MAX_FILENAME];
    strncpy(filename, token, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = 0;
    ensure_extension(filename, sizeof(filename));

    path_resolve(current_dir, filename, out_fullpath, VFS_MAX_PATH);
    return 1;
}

u8 shell_file_command(const char *args, const char *current_dir, u8 replace, u8 privileged) {
    if (!args || *args == 0) {
        kprintf("[FILE] Usage: file <name>[.ext] or file <path/to/name>[.ext]\n");
        kprintf("[FILE] Example: file Desktop/new/file.txt\n");
        return 0;
    }

    char local[512];
    strncpy(local, args, sizeof(local) - 1);
    local[sizeof(local) - 1] = 0;

    char *ptr = local;
    u8 any_success = 0;

    while (*ptr) {
        while (*ptr == ' ') ptr++;
        if (*ptr == 0) break;

        char *end = ptr;
        while (*end && *end != ' ') end++;

        char saved_char = *end;
        *end = 0;

        char fullpath[VFS_MAX_PATH];
        if (!build_path_and_filename(ptr, current_dir, fullpath)) {
            kprintf("[FILE] Invalid arguments for: %s\n", ptr);
            *end = saved_char;
            ptr = end + 1;
            continue;
        }

        if (!privileged && !auth_is_authorized() && is_root_child_path(fullpath)) {
            kprintf("[FILE] Permission denied: use 'rex file %s' to create or replace root-level files\n", fullpath);
            *end = saved_char;
            ptr = end + 1;
            continue;
        }

        char parent[VFS_MAX_PATH];
        get_parent_dir(fullpath, parent);
        if (!vfs_is_dir(parent)) {
            kprintf("[FILE] Directory does not exist: %s\n", parent);
            *end = saved_char;
            ptr = end + 1;
            continue;
        }

        vfs_entry_t *existing = vfs_find(fullpath);
        if (existing) {
            if (existing->is_dir) {
                kprintf("[FILE] Path is a directory: %s\n", fullpath);
            } else if (!replace) {
                kprintf("[FILE] File already exists: %s. Use 'rex file %s' to replace file content.\n", fullpath, fullpath);
            } else {
                if (vfs_create(fullpath, 1)) any_success = 1;
            }
        } else {
            if (vfs_create(fullpath, replace)) any_success = 1;
        }

        *end = saved_char;
        ptr = end + 1;
    }

    if (any_success) {
        vfs_fsync();  /* Ensure files are written to disk */
    }
    
    return any_success;
}
