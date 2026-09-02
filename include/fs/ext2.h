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

#ifndef EXT2_H
#define EXT2_H

#include "common.h"

/* Simplified EXT2 filesystem driver */

#define EXT2_SIGNATURE 0xEF53
#define EXT2_BLOCK_SIZE 1024
#define EXT2_INODE_SIZE 128

/* Superblock structure */
typedef struct {
    u32 inodes_count;
    u32 blocks_count;
    u32 r_blocks_count;
    u32 free_blocks_count;
    u32 free_inodes_count;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_frag_size;
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mnt_count;
    u16 max_mnt_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_rev_level;
    u32 lastcheck;
    u32 checkinterval;
    u32 creator_os;
    u32 rev_level;
    u16 def_resuid;
    u16 def_resgid;
} ext2_superblock_t;

/* Inode structure */
typedef struct {
    u16 mode;
    u16 uid;
    u32 size;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 links_count;
    u32 blocks;
    u32 flags;
    u32 osd1;
    u32 block[15];
    u32 generation;
    u32 file_acl;
    u32 dir_acl;
    u32 faddr;
    u32 osd2[3];
} ext2_inode_t;

/* Group descriptor structure */
typedef struct {
    u32 bg_block_bitmap;
    u32 bg_inode_bitmap;
    u32 bg_inode_table;
    u16 bg_free_blocks_count;
    u16 bg_free_inodes_count;
    u16 bg_used_dirs_count;
    u16 bg_pad;
    u32 bg_reserved[3];
} ext2_group_desc_t;

/* Directory entry */
typedef struct {
    u32 inode;
    u16 rec_len;
    u8 name_len;
    u8 file_type;
    char name[255];
} ext2_dirent_t;

/* Initialize EXT2 filesystem */
i32 ext2_init(void);

/* Set the filesystem base LBA on disk (partition start) */
void ext2_set_partition_lba(u32 lba);

/* Read inode */
i32 ext2_read_inode(u32 inode_num, ext2_inode_t *inode);

/* Read data block */
i32 ext2_read_block(u32 block_num, void *buffer);

/* Write data block */
i32 ext2_write_block(u32 block_num, const void *buffer);

/* Get current EXT2 block size */
u32 ext2_get_block_size(void);

/* Read file data from disk */
i32 ext2_read_data(u32 inode_num, void *buffer, u32 size, u32 offset);

/* Find inode by path */
u32 ext2_find_inode(const char *path);

/* Write operations */
i32 ext2_write_inode(u32 inode_num, ext2_inode_t *inode);
i32 ext2_create_file(const char *path, u32 mode);
i32 ext2_create_directory(const char *path, u32 mode);
i32 ext2_alloc_block(void);
i32 ext2_alloc_inode(void);
i32 ext2_add_directory_entry(u32 dir_inode, const char *name, u32 child_inode);
i32 ext2_remove_directory_entry(u32 dir_inode, const char *name);
i32 ext2_update_inode_size(u32 inode_num, u32 new_size);
i32 ext2_write_data(u32 inode_num, const void *buffer, u32 size);

/* Sync operations to force disk writes */
i32 ext2_fsync(void);

i32 ext2_unlink(const char *path);
i32 ext2_rmdir(const char *path);

#endif /* EXT2_H */// Add missing declaration
i32 ext2_remove_directory_entry(u32 dir_inode, const char *name);
