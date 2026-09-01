#ifndef VFS_H
#define VFS_H

#include "common.h"
#include <stddef.h>

/* Virtual Filesystem Layer with directory support */

#define VFS_MAGIC 0xDEADBEEF
#define VFS_VERSION 1
#define VFS_MAX_FILES 512
#define VFS_MAX_PATH 512
#define VFS_MAX_FILENAME 256
#define VFS_INVALID_FD 0xFFFFFFFFu

#define VFS_TYPE_MASK     0xF000
#define VFS_TYPE_UNKNOWN  0x0000
#define VFS_TYPE_FILE     0x8000
#define VFS_TYPE_DIR      0x4000
#define VFS_TYPE_SYMLINK  0xA000
#define VFS_TYPE_CHARDEV  0x2000

#define VFS_PERM_MASK           0x0FFF
#define VFS_PERM_FILE_DEFAULT   0644
#define VFS_PERM_DIR_DEFAULT    0755
#define VFS_PERM_SYMLINK        0777

#define VFS_O_RDONLY 0
#define VFS_O_WRONLY 1
#define VFS_O_RDWR   2
#define VFS_O_CREAT  0x100
#define VFS_O_TRUNC  0x200

#define VFS_DEVFS  2
#define VFS_PROCFS 3
#define VFS_SYSFS  4
#define VFS_TMPFS  5

typedef struct {
    char path[VFS_MAX_PATH];
    u32 size;
    u32 offset;
    u32 is_dir;
    u32 permissions;
} vfs_entry_t;

typedef struct {
    u32 magic;
    u32 version;
    u32 entry_count;
    u32 data_offset;
    vfs_entry_t entries[VFS_MAX_FILES];
} vfs_header_t;

typedef struct {
    char filename[VFS_MAX_FILENAME];
    u32 size;
    u32 is_dir;
} vfs_dirent_t;

/* Initialize VFS with initrd */
void vfs_init(void *initrd_addr);

/* Find entry by path (file or directory) */
vfs_entry_t *vfs_find(const char *path);

/* Read file by path */
u32 vfs_read(const char *path, void *buffer, u32 size);

/* Read from an open file descriptor */
u32 vfs_read_fd(u32 fd, void *buffer, u32 size);

/* Seek within an open file */
u32 vfs_lseek(u32 fd, i32 offset, i32 whence);

/* Get file stat information */
u32 vfs_stat(const char *path, void *statbuf);

/* List directory */
void vfs_listdir(const char *path);
void vfs_listdir_options(const char *path, u8 show_all, u8 human_readable);

/* List all files in filesystem */
void vfs_listall(void);

extern vfs_header_t *vfs_root;

/* Get entry info */
u32 vfs_size(const char *path);
u32 vfs_is_dir(const char *path);
const char *vfs_basename(const char *path);

/* Statistics */
void vfs_stats(void);
void vfs_debug(void);

/* Advanced operations */
void vfs_tree(void);
void vfs_tree_dir(const char *path);
u32 vfs_count_files(const char *path);
u32 vfs_count_dirs(const char *path);
u32 vfs_dir_size(const char *path);

/* Directory creation/deletion */
 u32 vfs_mkdir(const char *path, u8 force);
 u32 vfs_rmdir(const char *path);

/* File creation and removal */
 u32 vfs_create(const char *path, u8 force);
 u32 vfs_unlink(const char *path);
 u32 vfs_move(const char *source, const char *target);
 u32 vfs_open(const char *path);
 u32 vfs_retain(u32 fd);
 u32 vfs_close(u32 fd);
 u32 vfs_write(u32 fd, const void *buffer, u32 size);
 u32 vfs_remove_recursive(const char *path);
 u32 vfs_remove_dir_contents(const char *path);
 u32 vfs_cleanup_old_recycle_bin(const char *path, u32 age_ticks);

/* Sync operations - force filesystem to disk */
 u32 vfs_fsync(void);

/* File descriptor structure */
typedef struct {
    u32 inode;
    u32 offset;
    u32 flags;
} vfs_fd_t;

/* Mount point structure */
typedef struct {
    char mountpoint[256];
    u32 device;  /* 0 for RAM, 1 for disk */
    u32 start_block;
} vfs_mount_t;

/* Global file descriptor table */
#define MAX_FDS 32
extern vfs_fd_t fd_table[MAX_FDS];

/* Create a symlink */
u32 vfs_symlink(const char *target, const char *linkpath, u8 force);

/* Read the contents of a symlink */
u32 vfs_readlink(const char *path, char *buffer, u32 size);

/* Change file permissions */
i32 vfs_chmod(const char *path, u32 mode);

/* Mount filesystem */
i32 vfs_mount(const char *mountpoint, u32 device);

/* Unmount filesystem */
i32 vfs_unmount(const char *mountpoint);

#endif /* VFS_H */
