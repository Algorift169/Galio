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

#include "write.h"
#include "editor.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"
#include "auth.h"

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

static u8 is_auth_data_path(const char *path) {
    char normalized[VFS_MAX_PATH];
    if (!path_normalize(path, normalized, VFS_MAX_PATH)) {
        return 0;
    }

    return strcmp(normalized, "./var/account/udata.galio") == 0 ||
            strcmp(normalized, "./var/account/.udata.galio") == 0 ||
           strcmp(normalized, "var/account/udata.galio") == 0 ||
            strcmp(normalized, "var/account/.udata.galio") == 0 ||
            strcmp(normalized, "/var/account/udata.galio") == 0 ||
            strcmp(normalized, "/var/account/.udata.galio") == 0;
}

static void build_filepath(const char *args, const char *current_dir, char *out_path) {
    char filename[256];
    char *path_token = NULL;

    /* Copy arguments and trim */
    strncpy(filename, args, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = 0;
    
    /* Remove leading/trailing spaces */
    char *p = filename;
    while (*p == ' ') p++;
    
    if (*p == 0) {
        out_path[0] = 0;
        return;
    }

    char *end = p + strlen(p) - 1;
    while (end > p && *end == ' ') end--;
    *(end + 1) = 0;
    
    /* Split into filename and optional path token by space */
    char *space = p;
    while (*space && *space != ' ') space++;
    if (*space == ' ') {
        *space = 0;
        path_token = space + 1;
        while (*path_token == ' ') path_token++;
        if (*path_token == 0) path_token = NULL;
    }
    
    /* Determine target path and build resolved filename path */
    char target_dir[256];
    if (path_token) {
        path_resolve(current_dir, path_token, target_dir, sizeof(target_dir));
    } else {
        strncpy(target_dir, current_dir, sizeof(target_dir) - 1);
        target_dir[sizeof(target_dir) - 1] = 0;
    }

    /* Build full path: target_dir + '/' + filename */
    char combined[256];
    strncpy(combined, target_dir, sizeof(combined) - 1);
    combined[sizeof(combined) - 1] = 0;
    u32 combined_len = strlen(combined);
    if (combined_len > 0 && combined[combined_len - 1] != '/') {
        safe_strcat(combined, "/", sizeof(combined));
    }
    safe_strcat(combined, p, sizeof(combined));
    path_resolve(current_dir, combined, out_path, VFS_MAX_PATH);
}

u8 shell_write_command(const char *args, const char *current_dir, u8 privileged) {
    if (!args || *args == 0) {
        kprintf("[WRITE] Usage: write <filename> [path]\n");
        kprintf("[WRITE] Example: write test.txt ./usr/home/Documents\n");
        return 0;
    }

    char fullpath[256];
    build_filepath(args, current_dir, fullpath);
    if (fullpath[0] == 0) {
        kprintf("[WRITE] Usage: write <filename> [path]\n");
        return 0;
    }
    
    if (!privileged && !auth_is_authorized() && (is_root_child_path(fullpath) || is_auth_data_path(fullpath))) {
        kprintf("[WRITE] Permission denied: use 'rex write %s' to edit protected files\n", fullpath);
        return 0;
    }

    /* Ensure the file exists before editing */
    vfs_entry_t *entry = vfs_find(fullpath);
    if (!entry) {
        kprintf("[WRITE] File does not exist. Creating: %s\n", fullpath);
        if (!vfs_create(fullpath, 0)) {
            kprintf("[WRITE] Failed to create file: %s\n", fullpath);
            return 0;
        }
        kprintf("[WRITE] File created successfully.\n");
    } else if (entry->is_dir) {
        kprintf("[WRITE] Error: %s is a directory, not a file\n", fullpath);
        return 0;
    }
    
    return shell_editor(fullpath);
}
