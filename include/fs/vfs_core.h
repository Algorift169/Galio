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

#ifndef VFS_CORE_H
#define VFS_CORE_H

#include "vfs.h"
#include "common.h"
#include "dev/device.h"

#define VFS_TYPE_MASK 0xF000
#define VFS_TYPE_UNKNOWN 0x0000
#define VFS_TYPE_FILE 0x8000
#define VFS_TYPE_DIR 0x4000
#define VFS_TYPE_SYMLINK 0xA000
#define VFS_TYPE_CHARDEV 0x2000
#define EXT2_TYPE_DIR 0x4000

#define VFS_MAX_INODES 1024
#define VFS_MAX_DENTRIES 1024
#define VFS_MAX_FILE_HANDLES 64
#define VFS_MAX_DENTRY_CACHE 512
#define VFS_MAX_DATA_RAM (2 * 1024 * 1024)
#define VFS_RAMDISK_EXTRA (64 * 1024)
#define VFS_MAX_DIR_ENTRIES 64
#define VFS_MAX_BLOCKS 4

#define VFS_LOOKUP_PARENT 0x1
#define VFS_LOOKUP_NO_DISK 0x2
#define VFS_INVALID_FD 0xFFFFFFFFu

typedef struct vfs_core_dirent {
    u32 inode_number;
    char name[VFS_MAX_FILENAME];
} vfs_core_dirent_t;

typedef struct vfs_inode {
    u32 number;
    u32 mode;
    u32 size;
    u32 uid;
    u32 gid;
    u32 atime;
    u32 mtime;
    u32 ctime;
    u32 link_count;
    u32 block_count;
    u32 blocks[VFS_MAX_BLOCKS];
    u32 dirent_count;
    u32 dirent_capacity;
    vfs_core_dirent_t *dirents;
    u32 device_major;
    u32 device_minor;
    device_t *device;
} vfs_inode_t;

typedef struct vfs_dentry {
    char name[VFS_MAX_FILENAME];
    struct vfs_dentry *parent;
    struct vfs_dentry *first_child;
    struct vfs_dentry *next_sibling;
    struct vfs_dentry *hash_next;
    vfs_inode_t *inode;
} vfs_dentry_t;

typedef struct vfs_file {
    vfs_inode_t *inode;
    u32 pos;
    u32 flags;
    u32 ref_count;
    device_t *device;
} vfs_file_t;

void vfs_core_init(void *initrd_addr);
vfs_dentry_t *vfs_core_lookup(const char *path, u32 flags);
vfs_dentry_t *vfs_core_root(void);
void vfs_core_build_path(vfs_dentry_t *dentry, char *buffer);
u8 vfs_core_reload_root_from_disk(void);
vfs_inode_t *vfs_core_inode_by_number(u32 inode_number);
u32 vfs_core_open(const char *path);
u32 vfs_core_open_nodisk(const char *path);
u32 vfs_core_retain(u32 fd);
u32 vfs_core_close(u32 fd);
u32 vfs_core_write(u32 fd, const void *buffer, u32 size);
u32 vfs_core_read(u32 fd, void *buffer, u32 size);
u32 vfs_core_lseek(u32 fd, i32 offset, i32 whence);
u32 vfs_core_stat(const char *path, void *statbuf);
u32 vfs_core_stat_nodisk(const char *path, void *statbuf);
u32 vfs_core_read_path(const char *path, void *buffer, u32 size);
u32 vfs_core_read_path_nodisk(const char *path, void *buffer, u32 size);
u32 vfs_core_create_file(const char *path, u8 force);
u32 vfs_core_create_file_nodisk(const char *path, u8 force);
u32 vfs_core_create_dir(const char *path, u8 force);
u32 vfs_core_create_dir_nodisk(const char *path, u8 force);
u32 vfs_core_move(const char *src, const char *dest);
u32 vfs_core_rmdir(const char *path);
u32 vfs_core_unlink(const char *path);
u32 vfs_core_unlink_nodisk(const char *path);
u32 vfs_core_create_symlink(const char *target, const char *linkpath, u8 force);
u32 vfs_core_readlink(const char *path, char *buffer, u32 size);
u32 vfs_core_create_device(const char *path, u32 mode, u32 dev_id);
u32 vfs_core_create_device_ex(const char *path, u32 mode, u32 major, u32 minor);
u32 vfs_core_chmod(const char *path, u32 mode);
void vfs_core_init_disk_mode(void);
u8 vfs_core_reload_root_from_disk(void);
u8 vfs_core_is_disk_mode(void);
u8 vfs_core_is_directory(u32 inode_num);

#endif /* VFS_CORE_H */
