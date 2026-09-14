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
#include "vfs_core.h"
#include "auth.h"
#include "ext2.h"
#include "mm/heap.h"

static const char *skip_spaces(const char *text) {
    while (text && (*text == ' ' || *text == '\t')) text++;
    return text;
}

static u8 text_contains(const char *text, const char *needle) {
    u32 text_length;
    u32 needle_length;

    if (!text || !needle || !*needle) return 0;
    text_length = strlen(text);
    needle_length = strlen(needle);
    if (needle_length > text_length) return 0;
    for (u32 i = 0; i <= text_length - needle_length; i++) {
        if (strncmp(text + i, needle, needle_length) == 0) return 1;
    }
    return 0;
}

static void search_dentry_tree(vfs_dentry_t *dentry, const char *token,
                               u8 *found) {
    char path[VFS_MAX_PATH];

    if (!dentry || !dentry->inode) return;
    vfs_core_build_path(dentry, path);
    if (text_contains(dentry->name, token) || text_contains(path, token)) {
        kprintf("%s\n", path);
        *found = 1;
    }

    for (vfs_dentry_t *child = dentry->first_child; child; child = child->next_sibling) {
        search_dentry_tree(child, token, found);
    }
}

static void search_ext2_tree(u32 inode_num, const char *base_path,
                             const char *token, u8 *found) {
    ext2_inode_t inode;
    u32 block_size;
    u8 *buffer;

    if (ext2_read_inode(inode_num, &inode) != 0 ||
        !(inode.mode & 0x4000)) return;
    block_size = ext2_get_block_size();
    if (block_size == 0 || block_size > 65536) return;
    buffer = kmalloc(block_size);
    if (!buffer) return;

    for (u32 block = 0; block < 12 && inode.block[block]; block++) {
        if (ext2_read_block(inode.block[block], buffer) != 0) continue;
        u8 *cursor = buffer;
        u8 *end = buffer + block_size;
        while (cursor + sizeof(ext2_dirent_t) <= end) {
            ext2_dirent_t *dent = (ext2_dirent_t *)cursor;
            if (dent->rec_len == 0 || dent->rec_len > (u32)(end - cursor)) break;
            if (dent->inode && dent->name_len > 0 && dent->name_len < sizeof(dent->name)) {
                char name[VFS_MAX_FILENAME];
                char path[VFS_MAX_PATH];
                u32 name_length = dent->name_len;
                memcpy(name, dent->name, name_length);
                name[name_length] = 0;
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    strncpy(path, base_path, sizeof(path) - 1);
                    path[sizeof(path) - 1] = 0;
                    if (strlen(path) + name_length + 2 < sizeof(path)) {
                        strncat(path, "/", sizeof(path) - strlen(path) - 1);
                        strncat(path, name, sizeof(path) - strlen(path) - 1);
                        ext2_inode_t child;
                        if (ext2_read_inode(dent->inode, &child) == 0) {
                            if (text_contains(name, token) || text_contains(path, token)) {
                                kprintf("%s\n", path);
                                *found = 1;
                            }
                            if (child.mode & 0x4000) {
                                search_ext2_tree(dent->inode, path, token, found);
                            }
                        }
                    }
                }
            }
            cursor += dent->rec_len;
        }
    }
    kfree(buffer);
}

u8 shell_where_command(const char *args, const char *current_dir) {
    char token[VFS_MAX_PATH];
    const char *src = skip_spaces(args);
    u32 length = 0;
    u32 argument_length;
    (void)current_dir;

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
    argument_length = length;

    while (length > 1 && token[length - 1] == '/') {
        token[--length] = 0;
    }
    if (token[0] == '.' && token[1] == '/') {
        memmove(token, token + 2, length - 1);
    }

    if (src[argument_length] != 0) {
        const char *extra = skip_spaces(src + argument_length);
        if (*extra != 0) {
            kprintf("[WHERE] Usage: where <file-or-directory>\n");
            return 0;
        }
    }

    u8 found = 0;
    if (vfs_core_is_disk_mode()) {
        char base_path[VFS_MAX_PATH];
        path_normalize(current_dir ? current_dir : ".", base_path, sizeof(base_path));
        u32 inode_num = ext2_find_inode(base_path);
        if (inode_num) search_ext2_tree(inode_num, base_path, token, &found);
    } else {
        vfs_dentry_t *start = current_dir && *current_dir ?
                              vfs_core_lookup(current_dir, 0) : NULL;
        if (start) search_dentry_tree(start, token, &found);
    }

    if (!found) {
        kprintf("[WHERE] Not found: %s\n", token);
    }
    return found;
}
