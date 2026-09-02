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

#include "vfs_core.h"
#include "heap.h"
#include "kprintf.h"
#include "string.h"
#include "ext2.h"
#include "dev/device_manager.h"

#define VFS_INVALID_OFFSET 0xFFFFFFFFu

static vfs_inode_t vfs_inodes[VFS_MAX_INODES];
static vfs_dentry_t vfs_dentries[VFS_MAX_DENTRIES];
static vfs_file_t vfs_files[VFS_MAX_FILE_HANDLES];
static vfs_dentry_t *vfs_dentry_cache[VFS_MAX_DENTRY_CACHE];
static u8 *vfs_data_ram = NULL;
static u32 vfs_data_ram_size = 0;
static u32 vfs_data_top = 0;
static u32 vfs_next_inode = 0;
static u32 vfs_next_dentry = 0;
static vfs_dentry_t *vfs_root_dentry = NULL;
static vfs_inode_t *vfs_root_inode = NULL;

static u8 vfs_disk_mode = 0;

/* Public functions */
u8 vfs_core_is_disk_mode(void) { return vfs_disk_mode; }
void vfs_core_init_disk_mode(void) { vfs_disk_mode = 1; kprintf("[VFS] Switched to disk-backed mode (EXT2)\n"); }
void vfs_core_disable_disk_mode(void) { vfs_disk_mode = 0; kprintf("[VFS] Disk-backed mode disabled\n"); }

/* Check if an inode is a directory using EXT2 mode */
u8 vfs_core_is_directory(u32 inode_num) {
    if (vfs_disk_mode) {
        ext2_inode_t inode;
        if (ext2_read_inode(inode_num, &inode) == 0) {
            return (inode.mode & EXT2_TYPE_DIR) != 0;
        }
        return 0;
    }
    for (u32 i = 0; i < VFS_MAX_INODES; i++) {
        if (vfs_inodes[i].number == inode_num) {
            return (vfs_inodes[i].mode & VFS_TYPE_MASK) == VFS_TYPE_DIR;
        }
    }
    return 0;
}

/* Helper functions for RAM mode */
static void vfs_reset_cache(void) {
    for (u32 i = 0; i < VFS_MAX_DENTRY_CACHE; i++) vfs_dentry_cache[i] = NULL;
}

static void vfs_clear_root_children(void) {
    if (!vfs_root_dentry) return;
    vfs_root_dentry->first_child = NULL;
    vfs_reset_cache();
}

static void vfs_clear_root_inode_dirents(void) {
    if (!vfs_root_inode) return;
    if (vfs_root_inode->dirents) {
        kfree(vfs_root_inode->dirents);
        vfs_root_inode->dirents = NULL;
    }
    vfs_root_inode->dirent_count = 0;
    vfs_root_inode->dirent_capacity = 0;
}

static void vfs_reset_state(void) {
    vfs_next_inode = 0; vfs_next_dentry = 0; vfs_root_dentry = NULL; vfs_root_inode = NULL;
    for (u32 i = 0; i < VFS_MAX_INODES; i++) {
        vfs_inodes[i].number = 0xFFFFFFFFu;
        vfs_inodes[i].dirent_count = 0; vfs_inodes[i].dirent_capacity = 0; vfs_inodes[i].dirents = NULL;
    }
    for (u32 i = 0; i < VFS_MAX_DENTRIES; i++) {
        vfs_dentries[i].parent = NULL; vfs_dentries[i].first_child = NULL; vfs_dentries[i].next_sibling = NULL;
        vfs_dentries[i].hash_next = NULL; vfs_dentries[i].inode = NULL; vfs_dentries[i].name[0] = 0;
    }
    for (u32 i = 0; i < VFS_MAX_FILE_HANDLES; i++) {
        vfs_files[i].inode = NULL; vfs_files[i].pos = 0; vfs_files[i].flags = 0; vfs_files[i].ref_count = 0; vfs_files[i].device = NULL;
    }
    vfs_reset_cache();
}
static u32 vfs_hash_dentry(vfs_dentry_t *parent, const char *name) {
    u32 hash = 5381;
    if (parent && parent->inode) hash = hash * 33 + parent->inode->number;
    for (const char *p = name; *p; p++) hash = ((hash << 5) + hash) + (u8)*p;
    return hash % VFS_MAX_DENTRY_CACHE;
}
static vfs_dentry_t *vfs_cache_lookup(vfs_dentry_t *parent, const char *name) {
    if (!name) return NULL;
    u32 idx = vfs_hash_dentry(parent, name);
    for (u32 probe = 0; probe < VFS_MAX_DENTRY_CACHE; probe++) {
        u32 slot = (idx + probe) % VFS_MAX_DENTRY_CACHE;
        vfs_dentry_t *entry = vfs_dentry_cache[slot];
        if (entry && entry->parent == parent && strcmp(entry->name, name) == 0) return entry;
    }
    return NULL;
}
static void vfs_cache_insert(vfs_dentry_t *entry) {
    if (!entry) return;
    u32 idx = vfs_hash_dentry(entry->parent, entry->name);
    for (u32 probe = 0; probe < VFS_MAX_DENTRY_CACHE; probe++) {
        u32 slot = (idx + probe) % VFS_MAX_DENTRY_CACHE;
        if (!vfs_dentry_cache[slot] || vfs_dentry_cache[slot] == entry) {
            vfs_dentry_cache[slot] = entry; return;
        }
    }
    vfs_dentry_cache[idx] = entry;
}
static vfs_dentry_t *vfs_alloc_dentry(void) {
    if (vfs_next_dentry >= VFS_MAX_DENTRIES) return NULL;
    vfs_dentry_t *dentry = &vfs_dentries[vfs_next_dentry++];
    dentry->parent = NULL; dentry->first_child = NULL; dentry->next_sibling = NULL;
    dentry->hash_next = NULL; dentry->inode = NULL; dentry->name[0] = 0;
    return dentry;
}
static vfs_inode_t *vfs_alloc_inode(void) {
    if (vfs_next_inode >= VFS_MAX_INODES) return NULL;
    vfs_inode_t *inode = &vfs_inodes[vfs_next_inode];
    inode->number = vfs_next_inode;
    inode->mode = 0; inode->size = 0; inode->uid = 0; inode->gid = 0;
    inode->atime = 0; inode->mtime = 0; inode->ctime = 0; inode->link_count = 1;
    inode->block_count = 0; inode->dirent_count = 0; inode->dirent_capacity = 0; inode->dirents = NULL;
    inode->device_major = 0; inode->device_minor = 0; inode->device = NULL;
    for (u32 i = 0; i < VFS_MAX_BLOCKS; i++) inode->blocks[i] = VFS_INVALID_OFFSET;
    vfs_next_inode++;
    return inode;
}
static u32 vfs_allocate_data(u32 bytes) {
    if (!bytes) return 0;
    if (!vfs_data_ram) return 0xFFFFFFFFu;
    if (vfs_data_top + bytes > vfs_data_ram_size) return 0xFFFFFFFFu;
    u32 offset = vfs_data_top; vfs_data_top += bytes; return offset;
}
static u8 vfs_reserve_dirents(vfs_inode_t *inode, u32 required) {
    if (!inode || required <= inode->dirent_capacity) return 1;
    u32 new_capacity = inode->dirent_capacity ? inode->dirent_capacity * 2 : 8;
    while (new_capacity < required) new_capacity *= 2;
    vfs_core_dirent_t *new_block = krealloc(inode->dirents, new_capacity * sizeof(vfs_core_dirent_t));
    if (!new_block) return 0;
    inode->dirents = new_block; inode->dirent_capacity = new_capacity; return 1;
}
static vfs_core_dirent_t *vfs_find_dirent(vfs_inode_t *dir, const char *name) {
    if (!dir || !dir->dirents) return NULL;
    for (u32 i = 0; i < dir->dirent_count; i++)
        if (strcmp(dir->dirents[i].name, name) == 0) return &dir->dirents[i];
    return NULL;
}
static vfs_inode_t *vfs_find_inode_in_dir(vfs_inode_t *dir, const char *name) {
    vfs_core_dirent_t *dent = vfs_find_dirent(dir, name);
    if (!dent || dent->inode_number >= VFS_MAX_INODES) return NULL;
    return &vfs_inodes[dent->inode_number];
}
static u8 vfs_dirent_add(vfs_inode_t *dir, const char *name, u32 inode_number) {
    if (!dir || !name) return 0;
    if (!vfs_reserve_dirents(dir, dir->dirent_count + 1)) return 0;
    vfs_core_dirent_t *dent = &dir->dirents[dir->dirent_count++];
    dent->inode_number = inode_number; strncpy(dent->name, name, VFS_MAX_FILENAME - 1);
    dent->name[VFS_MAX_FILENAME - 1] = 0; dir->size = dir->dirent_count * sizeof(vfs_core_dirent_t);
    return 1;
}
static u8 vfs_dirent_remove(vfs_inode_t *dir, const char *name) {
    if (!dir || !name || !dir->dirents) return 0;
    for (u32 i = 0; i < dir->dirent_count; i++) {
        if (strcmp(dir->dirents[i].name, name) == 0) {
            for (u32 j = i+1; j < dir->dirent_count; j++) dir->dirents[j-1] = dir->dirents[j];
            dir->dirent_count--; dir->size = dir->dirent_count * sizeof(vfs_core_dirent_t);
            return 1;
        }
    }
    return 0;
}
static void vfs_link_child(vfs_dentry_t *parent, vfs_dentry_t *child) {
    if (!parent || !child) return;
    child->parent = parent; child->next_sibling = parent->first_child; parent->first_child = child;
    vfs_cache_insert(child);
}
static vfs_dentry_t *vfs_create_dentry(vfs_dentry_t *parent, const char *name, vfs_inode_t *inode) {
    vfs_dentry_t *dentry = vfs_alloc_dentry();
    if (!dentry) return NULL;
    dentry->inode = inode;
    if (name) { strncpy(dentry->name, name, VFS_MAX_FILENAME-1); dentry->name[VFS_MAX_FILENAME-1] = 0; }
    if (!parent) dentry->parent = dentry;
    else vfs_link_child(parent, dentry);
    vfs_cache_insert(dentry);
    return dentry;
}
static void vfs_build_path_from_dentry(vfs_dentry_t *dentry, char *buffer) {
    if (!dentry || !buffer) return;
    if (dentry == vfs_root_dentry) { buffer[0] = '.'; buffer[1] = 0; return; }
    char temp[VFS_MAX_PATH]; temp[0] = 0;
    vfs_dentry_t *walker = dentry;
    while (walker && walker != vfs_root_dentry) {
        char component[VFS_MAX_FILENAME];
        strncpy(component, walker->name, VFS_MAX_FILENAME-1); component[VFS_MAX_FILENAME-1] = 0;
        char next[VFS_MAX_PATH]; next[0] = 0;
        strncat(next, "/", VFS_MAX_PATH-1); strncat(next, component, VFS_MAX_PATH - strlen(next)-1);
        strncat(next, temp, VFS_MAX_PATH - strlen(next)-1);
        strncpy(temp, next, VFS_MAX_PATH-1); temp[VFS_MAX_PATH-1] = 0;
        walker = walker->parent;
    }
    if (temp[0] == 0) { buffer[0] = '.'; buffer[1] = 0; }
    else { strncpy(buffer, temp, VFS_MAX_PATH-1); buffer[VFS_MAX_PATH-1] = 0; }
}
static char *vfs_normalize(const char *path, char *buffer) {
    if (!path || !buffer) return NULL;
    u32 di = 0, i = 0;

    if (path[0] == '/') {
        buffer[di++] = '.';
        if (path[1] == '\0') {
            buffer[di] = 0;
            return buffer;
        }
        buffer[di++] = '/';
        i = 1;
    } else if (path[0] == '.' && (path[1] == '/' || path[1] == '\0')) {
        buffer[di++] = '.';
        if (path[1] == '\0') {
            buffer[di] = 0;
            return buffer;
        }
        buffer[di++] = '/';
        i = 2;
    }

    while (path[i] && di+1 < VFS_MAX_PATH) {
        if (path[i] == '/' && i>0 && path[i-1]=='/') { i++; continue; }
        buffer[di++] = path[i++];
    }
    buffer[di] = 0;
    if (di>1 && buffer[di-1]=='/') buffer[--di] = 0;
    if (di==0) { buffer[0]='.'; buffer[1]=0; }
    return buffer;
}
static void vfs_split_parent(const char *path, char *parent, char *name) {
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    if (strcmp(normalized, ".") == 0) {
        strcpy(parent, ".");
        name[0] = 0;
        return;
    }

    const char *slash = NULL;
    for (const char *p = normalized; *p; p++) {
        if (*p == '/') slash = p;
    }

    if (!slash) {
        strcpy(parent, ".");
        strncpy(name, normalized, VFS_MAX_FILENAME - 1);
        name[VFS_MAX_FILENAME - 1] = 0;
        return;
    }

    if (slash == normalized) {
        strcpy(parent, ".");
        strncpy(name, normalized + 1, VFS_MAX_FILENAME - 1);
        name[VFS_MAX_FILENAME - 1] = 0;
        return;
    }

    if (slash == normalized + 1 && normalized[0] == '.' && normalized[1] == '/') {
        strcpy(parent, ".");
        strncpy(name, normalized + 2, VFS_MAX_FILENAME - 1);
        name[VFS_MAX_FILENAME - 1] = 0;
        return;
    }

    u32 parent_len = slash - normalized;
    if (parent_len >= VFS_MAX_PATH) parent_len = VFS_MAX_PATH - 1;
    memcpy(parent, normalized, parent_len);
    parent[parent_len] = 0;
    strncpy(name, slash + 1, VFS_MAX_FILENAME - 1);
    name[VFS_MAX_FILENAME - 1] = 0;
}
static vfs_dentry_t *vfs_lookup_internal(const char *path) {
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    if (strcmp(normalized,".")==0) return vfs_root_dentry;
    vfs_dentry_t *current = vfs_root_dentry;
    const char *cursor = normalized;
    if (normalized[0] == '.' && normalized[1] == '/') cursor = normalized + 2;
    else if (normalized[0] == '/') cursor = normalized + 1;
    u32 depth = 0;
    while (*cursor && depth++ < 256) {
        const char *next = cursor;
        while (*next && *next != '/') next++;
        char component[VFS_MAX_FILENAME];
        u32 length = next - cursor;
        if (length >= VFS_MAX_FILENAME) length = VFS_MAX_FILENAME-1;
        memcpy(component, cursor, length); component[length] = 0;
        if (strcmp(component,".")==0) {}
        else if (strcmp(component,"..")==0) {
            if (current->parent && current->parent != current) current = current->parent;
        } else {
            if (!current->inode) return NULL;
            if (!vfs_core_is_directory(current->inode->number)) return NULL;
            vfs_dentry_t *child = vfs_cache_lookup(current, component);
            if (!child) {
                vfs_inode_t *child_inode = vfs_find_inode_in_dir(current->inode, component);
                if (!child_inode) {
                    if (vfs_disk_mode) {
                        char component_path[VFS_MAX_PATH];
                        u32 prefix_len = next - normalized;
                        if (prefix_len >= VFS_MAX_PATH) prefix_len = VFS_MAX_PATH - 1;
                        memcpy(component_path, normalized, prefix_len);
                        component_path[prefix_len] = 0;

                        u32 inode_num = ext2_find_inode(component_path);
                        if (inode_num == 0) return NULL;
                        child_inode = vfs_alloc_inode();
                        if (!child_inode) return NULL;
                        ext2_inode_t disk_inode;
                        if (ext2_read_inode(inode_num, &disk_inode) != 0) return NULL;
                        child_inode->number = inode_num;
                        child_inode->mode = (disk_inode.mode & EXT2_TYPE_DIR) ? VFS_TYPE_DIR : VFS_TYPE_FILE;
                        child_inode->size = disk_inode.size;
                        child_inode->block_count = disk_inode.blocks;
                        child_inode->link_count = disk_inode.links_count;
                        for (u32 j = 0; j < VFS_MAX_BLOCKS && j < 12; j++) {
                            child_inode->blocks[j] = disk_inode.block[j];
                        }
                    } else {
                        return NULL;
                    }
                }
                child = vfs_create_dentry(current, component, child_inode);
                if (!child) return NULL;
            }
            current = child;
        }
        if (*next == 0) break;
        cursor = next + 1;
    }
    if (depth >= 256) { kprintf("[VFS] Lookup depth exceeded: %s\n", path); return NULL; }
    return current;
}

static vfs_dentry_t *vfs_lookup_parent_internal(const char *path) {
    char parent[VFS_MAX_PATH], name[VFS_MAX_FILENAME];
    vfs_split_parent(path, parent, name);
    return vfs_lookup_internal(parent);
}
static vfs_dentry_t *vfs_make_directory_internal(const char *path, u8 force) {
    (void)force;
    if (!path) return NULL;
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    if (strcmp(normalized,".")==0) return vfs_root_dentry;
    char parent_path[VFS_MAX_PATH], name[VFS_MAX_FILENAME];
    vfs_split_parent(normalized, parent_path, name);
    if (vfs_disk_mode) {
        i32 inode_num = ext2_create_directory(normalized, 0x41ED);
        if (inode_num < 0) return NULL;
        vfs_dentry_t *parent = vfs_lookup_internal(parent_path);
        if (!parent) return NULL;
        vfs_inode_t *new_inode = vfs_alloc_inode();
        if (!new_inode) return NULL;
        new_inode->number = inode_num;
        new_inode->mode = VFS_TYPE_DIR;
        new_inode->size = 0;
        vfs_dentry_t *child = vfs_create_dentry(parent, name, new_inode);
        return child;
    } else {
        vfs_dentry_t *parent = vfs_lookup_internal(parent_path);
        if (!parent) {
            kprintf("[VFS DEBUG] make_directory: parent lookup failed for '%s' (parent='%s')\n", normalized, parent_path);
            return NULL;
        }
        if (!parent->inode) {
            kprintf("[VFS DEBUG] make_directory: parent has no inode for '%s' (parent='%s')\n", normalized, parent_path);
            return NULL;
        }
        if ((parent->inode->mode & VFS_TYPE_MASK) != VFS_TYPE_DIR) {
            kprintf("[VFS DEBUG] make_directory: parent is not a directory for '%s' (parent='%s', mode=0x%X)\n",
                    normalized, parent_path, parent->inode->mode);
            return NULL;
        }
        if (vfs_find_dirent(parent->inode, name)) {
            vfs_inode_t *existing = vfs_find_inode_in_dir(parent->inode, name);
            if (existing && existing->mode == VFS_TYPE_DIR) return parent;
            return NULL;
        }
        vfs_inode_t *inode = vfs_alloc_inode();
        if (!inode) {
            kprintf("[VFS DEBUG] make_directory: vfs_alloc_inode() failed for '%s'\n", normalized);
            return NULL;
        }
        inode->mode = VFS_TYPE_DIR;
        inode->size = 0; inode->link_count = 1; inode->dirent_count = 0;
        inode->dirent_capacity = 0; inode->dirents = NULL;
        vfs_dirent_add(inode, ".", inode->number);
        vfs_dirent_add(inode, "..", parent->inode->number);
        if (!vfs_dirent_add(parent->inode, name, inode->number)) {
            kprintf("[VFS DEBUG] make_directory: vfs_dirent_add to parent failed for '%s' (parent='%s')\n", normalized, parent_path);
            return NULL;
        }
        vfs_dentry_t *child = vfs_create_dentry(parent, name, inode);
        if (!child) {
            kprintf("[VFS DEBUG] make_directory: vfs_create_dentry failed for '%s'\n", normalized);
        }
        return child;
    }
}

// Core function to create a file or symlink, with optional data and disk mode support
static vfs_dentry_t *vfs_make_node_internal(const char *path, u32 mode, const u8 *data, u32 size, u32 dev_id, u8 force) {
    if (!path) return NULL;
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    char parent_path[VFS_MAX_PATH], name[VFS_MAX_FILENAME];
    vfs_split_parent(normalized, parent_path, name);
    if (vfs_disk_mode) {
        if ((mode & VFS_TYPE_MASK) == VFS_TYPE_FILE) {
            i32 inode_num = ext2_find_inode(normalized);
            if (inode_num == 0 || force) {
                inode_num = ext2_create_file(normalized, 0x81A4);
                if (inode_num < 0) return NULL;
            }
            if (data && size > 0) {
                if (ext2_write_data(inode_num, data, size) < 0) return NULL;
            }
            vfs_dentry_t *parent = vfs_lookup_internal(parent_path);
            if (!parent) return NULL;
            vfs_inode_t *new_inode = vfs_alloc_inode();
            if (!new_inode) return NULL;
            ext2_inode_t disk_inode;
            if (ext2_read_inode(inode_num, &disk_inode) != 0) return NULL;
            new_inode->number = inode_num;
            new_inode->mode = VFS_TYPE_FILE;
            new_inode->size = disk_inode.size;
            new_inode->block_count = disk_inode.blocks;
            new_inode->link_count = disk_inode.links_count;
            for (u32 i = 0; i < VFS_MAX_BLOCKS && i < 12; i++) {
                new_inode->blocks[i] = disk_inode.block[i];
            }
            vfs_dentry_t *child = vfs_create_dentry(parent, name, new_inode);
            return child;
        }
        return NULL;
    } else {
        vfs_dentry_t *parent = vfs_lookup_internal(parent_path);
        if (!parent || !parent->inode || parent->inode->mode != VFS_TYPE_DIR) return NULL;
        vfs_inode_t *existing = vfs_find_inode_in_dir(parent->inode, name);
        if (existing) {
            if (existing->mode == VFS_TYPE_DIR) return NULL;
            if (!force) return NULL;
            if ((mode & VFS_TYPE_MASK) == VFS_TYPE_FILE || (mode & VFS_TYPE_MASK) == VFS_TYPE_SYMLINK) {
                if (size > 0 && existing->blocks[0] != VFS_INVALID_OFFSET && size <= existing->size) {
                    if (data) memcpy(vfs_data_ram + existing->blocks[0], data, size);
                } else {
                    u32 new_offset = vfs_allocate_data(size);
                    if (new_offset == 0xFFFFFFFFu) return NULL;
                    if (data && size) memcpy(vfs_data_ram + new_offset, data, size);
                    existing->blocks[0] = new_offset;
                }
                existing->block_count = 1;
                existing->size = size;
            } else if ((mode & VFS_TYPE_MASK) == VFS_TYPE_CHARDEV) {
                existing->blocks[0] = dev_id;
                existing->block_count = 0;
                existing->size = 0;
            }
            existing->mode = mode;
            return vfs_cache_lookup(parent, name);
        }
        vfs_inode_t *inode = vfs_alloc_inode();
        if (!inode) return NULL;
        inode->mode = mode;
        inode->size = size;
        inode->link_count = 1;
        inode->block_count = 0;
        for (u32 i = 0; i < VFS_MAX_BLOCKS; i++) inode->blocks[i] = VFS_INVALID_OFFSET;
        if ((mode & VFS_TYPE_MASK) == VFS_TYPE_FILE) {
            if (size > 0) {
                u32 data_offset = vfs_allocate_data(size);
                if (data_offset == 0xFFFFFFFFu) return NULL;
                if (data) memcpy(vfs_data_ram + data_offset, data, size);
                else memset(vfs_data_ram + data_offset, 0, size);
                inode->blocks[0] = data_offset;
                inode->block_count = 1;
            }
        } else if ((mode & VFS_TYPE_MASK) == VFS_TYPE_SYMLINK) {
            if (size > 0) {
                u32 data_offset = vfs_allocate_data(size);
                if (data_offset == 0xFFFFFFFFu) return NULL;
                memcpy(vfs_data_ram + data_offset, data, size);
                inode->blocks[0] = data_offset;
                inode->block_count = 1;
            }
        } else if ((mode & VFS_TYPE_MASK) == VFS_TYPE_CHARDEV) {
            inode->blocks[0] = dev_id;
            inode->block_count = 0;
            inode->size = 0;
        }
        if (!vfs_dirent_add(parent->inode, name, inode->number)) return NULL;
        vfs_dentry_t *child = vfs_create_dentry(parent, name, inode);
        return child;
    }
}

// Convenience wrappers for common node types
static vfs_dentry_t *vfs_make_file_internal(const char *path, u8 force, const u8 *data, u32 size) {
    return vfs_make_node_internal(path, VFS_TYPE_FILE | VFS_PERM_FILE_DEFAULT, data, size, 0, force);
}
static void vfs_init_root(void) {
    vfs_root_inode = vfs_alloc_inode();
    if (!vfs_root_inode) return;
    vfs_root_inode->mode = VFS_TYPE_DIR | VFS_PERM_DIR_DEFAULT;
    vfs_root_inode->size = 0; vfs_root_inode->link_count = 1;
    vfs_root_inode->dirent_count = 0; vfs_root_inode->dirent_capacity = 0; vfs_root_inode->dirents = NULL;
    for (u32 i = 0; i < VFS_MAX_BLOCKS; i++) vfs_root_inode->blocks[i] = VFS_INVALID_OFFSET;
    vfs_root_dentry = vfs_create_dentry(NULL, "", vfs_root_inode);
    if (vfs_root_dentry) vfs_root_dentry->parent = vfs_root_dentry;
    vfs_dirent_add(vfs_root_inode, ".", vfs_root_inode->number);
    vfs_dirent_add(vfs_root_inode, "..", vfs_root_inode->number);
}

// Build the in-memory VFS structure from the initrd header
static void vfs_build_from_initrd(vfs_header_t *header) {
    if (!header) return;
    u32 total_data = 0;
    for (u32 i = 0; i < header->entry_count; i++) if (!header->entries[i].is_dir) total_data += header->entries[i].size;
    vfs_data_ram_size = total_data + VFS_RAMDISK_EXTRA;
    if (vfs_data_ram_size < header->data_offset + total_data) vfs_data_ram_size = header->data_offset + total_data + VFS_RAMDISK_EXTRA;
    vfs_data_ram = kmalloc(vfs_data_ram_size);
    if (!vfs_data_ram) { kprintf("[VFS] ERROR: Unable to allocate RAM disk buffer\n"); return; }
    vfs_data_top = 0;
    for (u32 i = 0; i < header->entry_count; i++) {
        vfs_entry_t *entry = &header->entries[i];
        if (!entry->path[0]) continue;
        if (entry->is_dir) { vfs_make_directory_internal(entry->path, 1); continue; }
        char parent[VFS_MAX_PATH], name[VFS_MAX_FILENAME];
        vfs_split_parent(entry->path, parent, name);
        if (parent[0] != 0 && strcmp(parent,".")!=0) vfs_make_directory_internal(parent, 1);
        u32 size = entry->size;
        u8 *source = (u8 *)header + entry->offset;
        vfs_make_file_internal(entry->path, 1, source, size);
    }
}

static int simple_itoa(u32 num, char *buf, int max_len) {
    if (max_len < 2) return 0;
    char temp[16];
    int i = 0;
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (num > 0 && i < 15) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    int j = 0;
    for (int k = i - 1; k >= 0; k--) {
        if (j < max_len - 1) buf[j++] = temp[k];
    }
    buf[j] = '\0';
    return j;
}

// Public API implementations 
void vfs_core_init(void *initrd_addr) {
    if (!initrd_addr) { kprintf("[VFS] ERROR: No initrd address supplied\n"); return; }
    vfs_header_t *header = (vfs_header_t *)initrd_addr;
    if (header->magic != VFS_MAGIC) { kprintf("[VFS] ERROR: Invalid initrd magic %08X\n", header->magic); return; }
    vfs_reset_state();
    vfs_init_root();
    if (!vfs_root_dentry) { kprintf("[VFS] ERROR: Root directory initialization failed\n"); return; }
    vfs_build_from_initrd(header);
    
    /* Always create system info virtual nodes under /proc */
    vfs_core_create_dir("./proc", 1);
    vfs_core_create_device("./proc/cpu", 0444, 10);
    vfs_core_create_device("./proc/mem", 0444, 11);
    vfs_core_create_device("./proc/battery", 0444, 12);

    if (!vfs_disk_mode) {
        vfs_core_create_dir("./sys", 1);
        vfs_core_create_dir("./tmp", 1);
    }
    kprintf("[VFS] Core filesystem initialized in RAM\n");
}
vfs_dentry_t *vfs_core_lookup(const char *path, u32 flags) {
    if (!path) return NULL;
    if (flags & VFS_LOOKUP_PARENT) return vfs_lookup_parent_internal(path);
    return vfs_lookup_internal(path);
}
vfs_dentry_t *vfs_core_root(void) { return vfs_root_dentry; }
void vfs_core_build_path(vfs_dentry_t *dentry, char *buffer) { vfs_build_path_from_dentry(dentry, buffer); }

static u32 vfs_core_open_internal(const char *path) {
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return VFS_INVALID_FD;
    u32 type = dentry->inode->mode & VFS_TYPE_MASK;
    if (type != VFS_TYPE_FILE && type != VFS_TYPE_CHARDEV && type != VFS_TYPE_SYMLINK) return VFS_INVALID_FD;
    if (type == VFS_TYPE_SYMLINK) {
        char target[VFS_MAX_PATH];
        if (!vfs_core_readlink(path, target, sizeof(target))) return VFS_INVALID_FD;
        return vfs_core_open_internal(target);
    }
    for (u32 i = 0; i < VFS_MAX_FILE_HANDLES; i++) {
        if (vfs_files[i].ref_count == 0) {
            vfs_files[i].inode = dentry->inode;
            vfs_files[i].pos = 0; vfs_files[i].flags = 0; vfs_files[i].ref_count = 1;
            vfs_files[i].device = (type == VFS_TYPE_CHARDEV) ?
                device_lookup_id(dentry->inode->device_major, dentry->inode->device_minor) : NULL;
            if (vfs_files[i].device && vfs_files[i].device->ops->open &&
                vfs_files[i].device->ops->open(vfs_files[i].device) != 0) {
                vfs_files[i].inode = NULL;
                vfs_files[i].device = NULL;
                vfs_files[i].ref_count = 0;
                return VFS_INVALID_FD;
            }
            return i;
        }
    }
    return VFS_INVALID_FD;
}

u32 vfs_core_open(const char *path) {
    return vfs_core_open_internal(path);
}
u32 vfs_core_retain(u32 fd) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    if (vfs_files[fd].ref_count == 0 || !vfs_files[fd].inode) return 0;
    vfs_files[fd].ref_count++;
    return 1;
}
u32 vfs_core_close(u32 fd) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    if (vfs_files[fd].ref_count == 0) return 0;
    vfs_files[fd].ref_count--;
    if (vfs_files[fd].ref_count == 0) {
        if (vfs_files[fd].device && vfs_files[fd].device->ops->close)
            vfs_files[fd].device->ops->close(vfs_files[fd].device);
        vfs_files[fd].inode = NULL; vfs_files[fd].pos = 0; vfs_files[fd].flags = 0;
        vfs_files[fd].device = NULL;
    }
    return 1;
}
u32 vfs_core_write(u32 fd, const void *buffer, u32 size) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    vfs_file_t *fh = &vfs_files[fd];
    if (!fh->inode || fh->ref_count == 0) return 0;
    u32 type = fh->inode->mode & VFS_TYPE_MASK;
    if (type == VFS_TYPE_CHARDEV) {
        if (!fh->device || !fh->device->ops->write) return 0;
        device_ssize_t written = fh->device->ops->write(fh->device, buffer, size);
        return written < 0 ? 0 : (u32)written;
    }
    if (type != VFS_TYPE_FILE) return 0;
    if (vfs_disk_mode) {
        i32 written = ext2_write_data(fh->inode->number, buffer, size);
        if (written < 0) return 0;
        fh->inode->size = written;
        fh->pos = written;
        return written;
    } else {
        u32 offset = fh->inode->blocks[0];
        if (offset == VFS_INVALID_OFFSET || size > fh->inode->size) {
            if (size > 0) {
                offset = vfs_allocate_data(size);
                if (offset == 0xFFFFFFFFu) return 0;
                fh->inode->blocks[0] = offset;
                fh->inode->block_count = 1;
            } else {
                fh->inode->blocks[0] = VFS_INVALID_OFFSET;
                fh->inode->block_count = 0;
            }
        }
        if (size > 0) {
            memcpy(vfs_data_ram + offset, buffer, size);
        }
        fh->inode->size = size;
        fh->pos = size;
        return size;
    }
}
u32 vfs_core_read_path(const char *path, void *buffer, u32 size) {
    if (!path || !buffer) return 0;
    
    /* Pre-check for our virtual proc nodes to work in both disk/non-disk modes */
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (dentry && dentry->inode && (dentry->inode->mode & VFS_TYPE_MASK) == VFS_TYPE_CHARDEV) {
        u32 dev_id = dentry->inode->blocks[0];
        if (dev_id >= 10 && dev_id <= 12) {
            if (dev_id == 10) {
                /* /proc/cpu */
                extern u8 process_get_cpu_usage(void);
                u8 usage = process_get_cpu_usage();
                char temp[16];
                int len = simple_itoa(usage, temp, sizeof(temp));
                if (len < 14) {
                    temp[len++] = '%';
                    temp[len++] = '\n';
                    temp[len] = '\0';
                }
                if (size > (u32)len) size = len;
                memcpy(buffer, temp, size);
                return size;
            } else if (dev_id == 11) {
                /* /proc/mem */
                extern u32 heap_get_used_memory(void);
                extern u32 heap_get_total_memory(void);
                u32 used = heap_get_used_memory();
                u32 total = heap_get_total_memory();
                if (total == 0) total = 1;
                u32 usage = (used * 100) / total;
                char temp[16];
                int len = simple_itoa(usage, temp, sizeof(temp));
                if (len < 14) {
                    temp[len++] = '%';
                    temp[len++] = '\n';
                    temp[len] = '\0';
                }
                if (size > (u32)len) size = len;
                memcpy(buffer, temp, size);
                return size;
            } else if (dev_id == 12) {
                /* /proc/battery */
                char temp[16];
                int len = simple_itoa(85, temp, sizeof(temp));
                if (len < 14) {
                    temp[len++] = '%';
                    temp[len++] = '\n';
                    temp[len] = '\0';
                }
                if (size > (u32)len) size = len;
                memcpy(buffer, temp, size);
                return size;
            }
        }
    }

    if (vfs_disk_mode) {
        vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
        if (!dentry || !dentry->inode) return 0;
        if ((dentry->inode->mode & VFS_TYPE_MASK) == VFS_TYPE_SYMLINK) {
            char target[VFS_MAX_PATH];
            if (!vfs_core_readlink(path, target, sizeof(target))) return 0;
            return vfs_core_read_path(target, buffer, size);
        }
        if ((dentry->inode->mode & VFS_TYPE_MASK) != VFS_TYPE_FILE) return 0;
        i32 result = ext2_read_data(dentry->inode->number, buffer, size, 0);
        return result > 0 ? (u32)result : 0;
    }
    dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    u32 type = dentry->inode->mode & VFS_TYPE_MASK;
    if (type == VFS_TYPE_SYMLINK) {
        char target[VFS_MAX_PATH];
        if (!vfs_core_readlink(path, target, sizeof(target))) return 0;
        return vfs_core_read_path(target, buffer, size);
    }
    if (type == VFS_TYPE_CHARDEV) {
        u32 dev_id = dentry->inode->blocks[0];
        if (dev_id == 1) {
            /* /dev/null reads EOF */
            return 0;
        } else if (dev_id == 2) {
            memset(buffer, 0, size);
            return size;
        } else if (dev_id == 3) {
            u8 *out = buffer;
            for (u32 i = 0; i < size; i++) {
                out[i] = (u8)((i * 37) ^ 0xA5);
            }
            return size;
        }
        return 0;
    }
    if (type != VFS_TYPE_FILE) return 0;
    u32 to_read = size;
    if (to_read > dentry->inode->size) to_read = dentry->inode->size;
    if (to_read == 0) return 0;
    u32 offset = dentry->inode->blocks[0];
    if (offset == 0xFFFFFFFFu) return 0;
    memcpy(buffer, vfs_data_ram + offset, to_read);
    return to_read;
}
u32 vfs_core_create_file(const char *path, u8 force) {
    if (vfs_disk_mode) {
        if (!path) return 0;
        u32 existing_inode = ext2_find_inode(path);
        if (existing_inode != 0) {
            if (!force) return 0;
            return ext2_update_inode_size(existing_inode, 0) == 0 ? 1 : 0;
        }
        i32 inode_num = ext2_create_file(path, 0x81A4);
        return inode_num > 0 ? 1 : 0;
    }
    vfs_dentry_t *created = vfs_make_file_internal(path, force, NULL, 0);
    return created != NULL;
}
u32 vfs_core_create_dir(const char *path, u8 force) {
    if (vfs_disk_mode) {
        if (!path) return 0;
        u32 existing_inode = ext2_find_inode(path);
        if (existing_inode != 0) {
            if (!force) return 0;
            ext2_inode_t inode;
            if (ext2_read_inode(existing_inode, &inode) != 0) return 0;
            return (inode.mode & 0x4000) ? 1 : 0;
        }
        /* Ensure parent directories exist when forcing creation on disk */
        if (force) {
            /* compute parent path (strip trailing slashes first) */
            char parent[256];
            if (!path) return 0;
            u32 plen = strlen(path);
            while (plen > 1 && path[plen - 1] == '/') plen--;
            int pos = (int)plen - 1;
            while (pos > 0 && path[pos] != '/') pos--;
            if (pos <= 0) {
                /* parent is root or current; nothing to do */
            } else {
                u32 copy_len = (u32)pos;
                if (copy_len >= sizeof(parent)) copy_len = sizeof(parent) - 1;
                memcpy(parent, path, copy_len);
                parent[copy_len] = '\0';
                if (ext2_find_inode(parent) == 0) {
                    /* recursively create parent */
                    if (!vfs_core_create_dir(parent, 1)) {
                        kprintf("[VFS] Failed to create parent dir: %s\n", parent);
                        return 0;
                    }
                }
            }
        }
        i32 inode_num = ext2_create_directory(path, 0x41ED);
        if (inode_num < 0) {
            kprintf("[VFS] ext2_create_directory('%s') failed (ret=%d)\n", path, inode_num);
            return 0;
        }
        return 1;
    }
    vfs_dentry_t *created = vfs_make_directory_internal(path, force);
    return created != NULL;
}

u32 vfs_core_create_symlink(const char *target, const char *linkpath, u8 force) {
    if (!target || !linkpath) return 0;
    if (vfs_disk_mode) return 0;
    u32 len = strlen(target) + 1;
    u32 mode = VFS_TYPE_SYMLINK | VFS_PERM_SYMLINK;
    vfs_dentry_t *created = vfs_make_node_internal(linkpath, mode, (const u8 *)target, len, 0, force);
    return created != NULL;
}

u32 vfs_core_readlink(const char *path, char *buffer, u32 size) {
    if (!path || !buffer || size == 0) return 0;
    if (vfs_disk_mode) return 0;
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    if ((dentry->inode->mode & VFS_TYPE_MASK) != VFS_TYPE_SYMLINK) return 0;
    u32 len = dentry->inode->size;
    if (len == 0) return 0;
    u32 to_copy = len;
    if (to_copy >= size) to_copy = size - 1;
    u32 offset = dentry->inode->blocks[0];
    if (offset == 0xFFFFFFFFu) return 0;
    memcpy(buffer, vfs_data_ram + offset, to_copy);
    buffer[to_copy] = '\0';
    return to_copy;
}

u32 vfs_core_create_device_ex(const char *path, u32 mode, u32 major, u32 minor) {
    if (!path) return 0;
    if (vfs_disk_mode) return 0;
    u32 full_mode = VFS_TYPE_CHARDEV | (mode & VFS_PERM_MASK);
    vfs_dentry_t *created = vfs_make_node_internal(path, full_mode, NULL, 0, (major << 16) | minor, 1);
    if (created && created->inode) {
        created->inode->device_major = major;
        created->inode->device_minor = minor;
        created->inode->device = device_lookup_id(major, minor);
    }
    return created != NULL;
}

u32 vfs_core_create_device(const char *path, u32 mode, u32 dev_id) {
    return vfs_core_create_device_ex(path, mode, dev_id >> 16, dev_id & 0xFFFFu);
}

u32 vfs_core_chmod(const char *path, u32 mode) {
    if (!path) return 0;
    if (vfs_disk_mode) return 0;
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    u32 type = dentry->inode->mode & VFS_TYPE_MASK;
    dentry->inode->mode = type | (mode & VFS_PERM_MASK);
    return 1;
}
vfs_inode_t *vfs_core_inode_by_number(u32 inode_number) {
    if (inode_number >= VFS_MAX_INODES) return NULL;
    return &vfs_inodes[inode_number];
}

static void vfs_unlink_child_dentry(vfs_dentry_t *parent, vfs_dentry_t *child) {
    if (!parent || !child) return;
    vfs_dentry_t **slot = &parent->first_child;
    while (*slot) {
        if (*slot == child) {
            *slot = child->next_sibling;
            child->next_sibling = NULL;
            child->parent = NULL;
            return;
        }
        slot = &(*slot)->next_sibling;
    }
}

u32 vfs_core_move(const char *src, const char *dest) {
    if (!src || !dest || strcmp(src, dest) == 0) return 0;
    if (vfs_disk_mode) {
        char src_parent[VFS_MAX_PATH], src_name[VFS_MAX_FILENAME];
        char dest_parent[VFS_MAX_PATH], dest_name[VFS_MAX_FILENAME];
        vfs_split_parent(src, src_parent, src_name);
        vfs_split_parent(dest, dest_parent, dest_name);
        if (src_name[0] == 0 || dest_name[0] == 0) return 0;
        if (ext2_find_inode(dest) != 0) return 0;
        u32 src_inode_num = ext2_find_inode(src);
        if (src_inode_num == 0) return 0;
        u32 src_parent_inode = ext2_find_inode(src_parent);
        u32 dest_parent_inode = ext2_find_inode(dest_parent);
        if (src_parent_inode == 0 || dest_parent_inode == 0) return 0;
        if (ext2_add_directory_entry(dest_parent_inode, dest_name, src_inode_num) != 0) return 0;
        if (ext2_remove_directory_entry(src_parent_inode, src_name) != 0) {
            ext2_remove_directory_entry(dest_parent_inode, dest_name);
            return 0;
        }
        return 1;
    }

    vfs_dentry_t *src_dentry = vfs_core_lookup(src, 0);
    if (!src_dentry || !src_dentry->inode) return 0;
    if (src_dentry == vfs_root_dentry) return 0;
    vfs_dentry_t *src_parent = src_dentry->parent;
    if (!src_parent || !src_parent->inode) return 0;
    char dest_parent[VFS_MAX_PATH], dest_name[VFS_MAX_FILENAME];
    vfs_split_parent(dest, dest_parent, dest_name);
    if (dest_name[0] == 0) return 0;
    vfs_dentry_t *dest_parent_dentry = vfs_core_lookup(dest_parent, 0);
    if (!dest_parent_dentry || !dest_parent_dentry->inode || dest_parent_dentry->inode->mode != VFS_TYPE_DIR) return 0;
    if (vfs_core_lookup(dest, 0)) return 0;
    if (!vfs_dirent_remove(src_parent->inode, src_dentry->name)) return 0;
    if (!vfs_dirent_add(dest_parent_dentry->inode, dest_name, src_dentry->inode->number)) {
        vfs_dirent_add(src_parent->inode, src_dentry->name, src_dentry->inode->number);
        return 0;
    }
    vfs_unlink_child_dentry(src_parent, src_dentry);
    strncpy(src_dentry->name, dest_name, VFS_MAX_FILENAME - 1);
    src_dentry->name[VFS_MAX_FILENAME - 1] = 0;
    vfs_link_child(dest_parent_dentry, src_dentry);
    return 1;
}

u32 vfs_core_unlink(const char *path) {
    if (vfs_disk_mode) {
        return ext2_unlink(path) == 0 ? 1 : 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    u32 type = dentry->inode->mode & VFS_TYPE_MASK;
    if (type == VFS_TYPE_DIR) return 0;
    if (dentry == vfs_root_dentry) return 0;
    vfs_dentry_t *parent = dentry->parent;
    if (!parent || !parent->inode) return 0;
    if (!vfs_dirent_remove(parent->inode, dentry->name)) return 0;
    dentry->inode->link_count--;
    if (dentry->inode->link_count == 0) dentry->inode->mode = 0;
    return 1;
}
u32 vfs_core_rmdir(const char *path) {
    if (vfs_disk_mode) {
        return ext2_rmdir(path) == 0 ? 1 : 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    if (dentry == vfs_root_dentry) return 0;
    u32 entries = 0;
    for (u32 i = 0; i < dentry->inode->dirent_count; i++) {
        const char *name = dentry->inode->dirents[i].name;
        if (strcmp(name,".")!=0 && strcmp(name,"..")!=0) entries++;
    }
    if (entries > 0) return 0;
    vfs_dentry_t *parent = dentry->parent;
    if (!parent || !parent->inode) return 0;
    if (!vfs_dirent_remove(parent->inode, dentry->name)) return 0;
    dentry->inode->mode = 0;
    return 1;
}
u32 vfs_core_read(u32 fd, void *buffer, u32 size) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    if (!buffer) return 0;
    vfs_file_t *fh = &vfs_files[fd];
    if (!fh->inode || fh->ref_count == 0) return 0;
    u32 type = fh->inode->mode & VFS_TYPE_MASK;
    if (type == VFS_TYPE_CHARDEV) {
        if (fh->device && fh->device->ops->read) {
            device_ssize_t result = fh->device->ops->read(fh->device, buffer, size);
            return result < 0 ? 0 : (u32)result;
        }
        u32 dev_id = fh->inode->blocks[0];
        if (dev_id == 1) {
            return 0;
        } else if (dev_id == 2) {
            memset(buffer, 0, size);
            return size;
        } else if (dev_id == 3) {
            u8 *out = buffer;
            for (u32 i = 0; i < size; i++) {
                out[i] = (u8)((i * 37) ^ 0xA5);
            }
            return size;
        }
        return 0;
    }
    if (type != VFS_TYPE_FILE) return 0;
    if (vfs_disk_mode) {
        if (fh->inode->number == 0) return 0;
        i32 result = ext2_read_data(fh->inode->number, buffer, size, fh->pos);
        if (result <= 0) return 0;
        fh->pos += result;
        return result;
    }
    u32 to_read = size;
    if (fh->pos >= fh->inode->size) return 0;
    if (fh->pos + to_read > fh->inode->size) to_read = fh->inode->size - fh->pos;
    if (to_read == 0) return 0;
    u32 offset = fh->inode->blocks[0];
    if (offset == 0xFFFFFFFFu) return 0;
    memcpy(buffer, vfs_data_ram + offset + fh->pos, to_read);
    fh->pos += to_read;
    return to_read;
}
u32 vfs_core_lseek(u32 fd, i32 offset, i32 whence) {
    if (fd >= VFS_MAX_FILE_HANDLES) return (u32)-1;
    vfs_file_t *fh = &vfs_files[fd];
    if (!fh->inode || fh->ref_count == 0 || fh->inode->mode != VFS_TYPE_FILE) return (u32)-1;
    u32 new_pos;
    switch (whence) {
        case 0: new_pos = offset; break;
        case 1: new_pos = fh->pos + offset; break;
        case 2: new_pos = fh->inode->size + offset; break;
        default: return (u32)-1;
    }
    fh->pos = new_pos;
    return new_pos;
}
u32 vfs_core_stat(const char *path, void *statbuf) {
    if (!path || !statbuf) return 0;
    if (vfs_disk_mode) {
        u32 inode_num = ext2_find_inode(path);
        if (inode_num == 0) return 0;
        ext2_inode_t inode;
        if (ext2_read_inode(inode_num, &inode) != 0) return 0;
        typedef struct { u32 mode; u32 size; u32 blocks; u32 atime; u32 mtime; u32 ctime; } simple_stat_t;
        simple_stat_t *stat = (simple_stat_t *)statbuf;
        stat->mode = (inode.mode & 0x4000) ? VFS_TYPE_DIR : VFS_TYPE_FILE;
        stat->size = inode.size; stat->blocks = inode.blocks;
        stat->atime = inode.atime; stat->mtime = inode.mtime; stat->ctime = inode.ctime;
        return 1;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    typedef struct { u32 mode; u32 size; u32 blocks; u32 atime; u32 mtime; u32 ctime; } simple_stat_t;
    simple_stat_t *stat = (simple_stat_t *)statbuf;
    stat->mode = dentry->inode->mode; stat->size = dentry->inode->size; stat->blocks = dentry->inode->block_count;
    stat->atime = dentry->inode->atime; stat->mtime = dentry->inode->mtime; stat->ctime = dentry->inode->ctime;
    return 1;
}

/* Reload root dentry from disk when switching to disk mode */
u8 vfs_core_reload_root_from_disk(void) {
    if (!vfs_disk_mode) return 0;
    
    ext2_inode_t root_inode;
    if (ext2_read_inode(2, &root_inode) != 0) {
        kprintf("[VFS] Failed to read root inode from disk\n");
        return 0;
    }
    
    vfs_clear_root_children();
    vfs_clear_root_inode_dirents();

    vfs_root_inode->number = 2;
    vfs_root_inode->mode = VFS_TYPE_DIR;
    vfs_root_inode->size = root_inode.size;
    vfs_root_inode->block_count = root_inode.blocks;
    vfs_root_inode->link_count = root_inode.links_count;
    for (u32 i = 0; i < VFS_MAX_BLOCKS && i < 12; i++) {
        vfs_root_inode->blocks[i] = root_inode.block[i];
    }
    
    kprintf("[VFS] Root dentry updated to disk inode 2 (mode=0x%x, size=%u)\n", root_inode.mode, root_inode.size);
    return 1;
}
