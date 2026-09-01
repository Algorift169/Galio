/* vfs.c - Virtual Filesystem Layer with normalized paths */
#include "vfs.h"
#include "vfs_core.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"

vfs_header_t *vfs_root = NULL;
static vfs_entry_t compat_entries[32];
static u32 compat_index = 0;

static vfs_entry_t *build_compat_entry(vfs_dentry_t *dentry, const char *path) {
    if (!dentry || !dentry->inode) return NULL;
    vfs_entry_t *entry = &compat_entries[compat_index++ % 32];
    char normalized[VFS_MAX_PATH];
    if (path && *path) {
        path_normalize(path, normalized, sizeof(normalized));
    } else {
        vfs_core_build_path(dentry, normalized);
    }
    strncpy(entry->path, normalized, VFS_MAX_PATH - 1);
    entry->path[VFS_MAX_PATH - 1] = 0;
    entry->size = dentry->inode->size;
    entry->is_dir = (dentry->inode->mode == VFS_TYPE_DIR);
    entry->offset = dentry->inode->blocks[0];
    entry->permissions = entry->is_dir ? 0755 : 0644;
    return entry;
}

void vfs_init(void *initrd_addr) {
    vfs_root = (vfs_header_t *)initrd_addr;

    if (!vfs_root) {
        kprintf("[VFS] ERROR: No initrd mounted\n");
        return;
    }

    if (vfs_root->magic != VFS_MAGIC) {
        kprintf("[VFS] ERROR: Invalid magic number %08X (expected %08X)\n",
                vfs_root->magic, VFS_MAGIC);
        vfs_root = NULL;
        return;
    }

    vfs_core_init(initrd_addr);
    kprintf("[VFS] ✓ Filesystem mounted successfully\n");
    kprintf("[VFS] - Magic: 0x%08X\n", vfs_root->magic);
    kprintf("[VFS] - Version: %u\n", vfs_root->version);
}

vfs_entry_t *vfs_find(const char *path) {
    if (!vfs_root || !path) return NULL;
    char norm_path[VFS_MAX_PATH];
    path_normalize(path, norm_path, sizeof(norm_path));
    vfs_dentry_t *dentry = vfs_core_lookup(norm_path, 0);
    return build_compat_entry(dentry, norm_path);
}

u32 vfs_read(const char *path, void *buffer, u32 size) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    return vfs_core_read_path(path, buffer, size);
}

static u32 path_depth(const char *path) {
    u32 depth = 0;
    for (u32 i = 0; path[i]; i++) {
        if (path[i] == '/') depth++;
    }
    return depth;
}

// Returns a pointer to a static buffer containing the normalized path. Caller should not modify or free this buffer.
const char *vfs_basename(const char *path) {
    static char buf[VFS_MAX_FILENAME];
    if (!path || *path == 0) {
        buf[0] = 0;
        return buf;
    }
    int i = __builtin_strlen(path) - 1;

    while (i > 0 && path[i] != '/') i--;
    if (path[i] == '/') i++;

    int j = 0;
    while (path[i] && j < VFS_MAX_FILENAME - 1) {
        buf[j++] = path[i++];
    }
    buf[j] = 0;
    return buf;
}

void vfs_listdir_options(const char *path, u8 show_all, u8 human_readable) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }

    char norm_path[VFS_MAX_PATH];
    path_normalize(path, norm_path, sizeof(norm_path));
    vfs_dentry_t *dentry = vfs_core_lookup(norm_path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) {
        kprintf("[VFS] ERROR: Directory not found: %s\n", path);
        return;
    }

    kprintf("Directory listing: %s\n", norm_path);
    u32 dir_count = 0;
    u32 file_count = 0;
    u32 total_dir_size = 0;

    for (u32 i = 0; i < dentry->inode->dirent_count; i++) {
        vfs_core_dirent_t *dent = &dentry->inode->dirents[i];
        if (!show_all && dent->name[0] == '.') continue;
        vfs_inode_t *child = vfs_core_inode_by_number(dent->inode_number);
        if (!child) continue;
        if (child->mode == VFS_TYPE_DIR) {
            kprintf("    [DIR]  %s/\n", dent->name);
            dir_count++;
        } else {
            if (human_readable && child->size >= 1024) {
                kprintf("    [FILE] %s (%u.%uK)\n", dent->name,
                        child->size / 1024, (child->size % 1024) * 10 / 1024);
            } else {
                kprintf("    [FILE] %s (%u bytes)\n", dent->name, child->size);
            }
            file_count++;
            total_dir_size += child->size;
        }
    }

    kprintf("    ─────────────────────────────────────────────\n");
    kprintf("    Dirs: %u | Files: %u | Total size: %u bytes\n",
            dir_count, file_count, total_dir_size);
}

void vfs_listdir(const char *path) {
    vfs_listdir_options(path, 0, 0);
}

static void vfs_print_tree_recursive(vfs_dentry_t *node, u32 depth) {
    if (!node || !node->inode) return;
    if (node != vfs_core_root()) {
        for (u32 d = 0; d < depth; d++) kprintf("  ");
        if (node->inode->mode == VFS_TYPE_DIR) {
            kprintf("├─ [DIR]  %s/\n", node->name);
        } else {
            kprintf("├─ [FILE] %s (%u bytes)\n", node->name, node->inode->size);
        }
    }
    for (vfs_dentry_t *child = node->first_child; child; child = child->next_sibling) {
        vfs_print_tree_recursive(child, node == vfs_core_root() ? 0 : depth + 1);
    }
}

void vfs_listall(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    kprintf("[VFS] Complete filesystem tree:\n");
    kprintf("================================================================\n");
    vfs_print_tree_recursive(vfs_core_root(), 0);
    kprintf("================================================================\n");
}

u32 vfs_size(const char *path) {
    vfs_entry_t *entry = vfs_find(path);
    return entry ? entry->size : 0;
}

u32 vfs_is_dir(const char *path) {
    vfs_entry_t *entry = vfs_find(path);
    return entry ? entry->is_dir : 0;
}

void vfs_stats(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    u32 dirs = 0, files = 0, total_data_size = 0;
    for (u32 i = 0; i < vfs_root->entry_count; i++) {
        if (vfs_root->entries[i].path[0] == 0) continue;
        if (vfs_root->entries[i].is_dir) dirs++;
        else {
            files++;
            total_data_size += vfs_root->entries[i].size;
        }
    }
    kprintf("[VFS] Filesystem Statistics:\n");
    kprintf("    Total entries:    %u\n", vfs_root->entry_count);
    kprintf("    Directories:      %u\n", dirs);
    kprintf("    Files:            %u\n", files);
    kprintf("    Total data size:  %u bytes (%.2f KB)\n",
            total_data_size, (float)total_data_size / 1024.0);
    kprintf("    Header magic:     0x%08X\n", vfs_root->magic);
    kprintf("    Header version:   %u\n", vfs_root->version);
}

void vfs_debug(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    return;
}

u32 vfs_count_files(const char *path) {
    if (!vfs_root) return 0;
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    u32 count = 0;
    for (u32 i = 0; i < dentry->inode->dirent_count; i++) {
        const char *name = dentry->inode->dirents[i].name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        vfs_inode_t *child = vfs_core_inode_by_number(dentry->inode->dirents[i].inode_number);
        if (child && child->mode != VFS_TYPE_DIR) {
            count++;
        }
    }
    return count;
}

u32 vfs_count_dirs(const char *path) {
    if (!vfs_root) return 0;
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    u32 count = 0;
    for (u32 i = 0; i < dentry->inode->dirent_count; i++) {
        const char *name = dentry->inode->dirents[i].name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        vfs_inode_t *child = vfs_core_inode_by_number(dentry->inode->dirents[i].inode_number);
        if (child && child->mode == VFS_TYPE_DIR) {
            count++;
        }
    }
    return count;
}

u32 vfs_dir_size(const char *path) {
    if (!vfs_root) return 0;
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    u32 size = 0;
    for (u32 i = 0; i < dentry->inode->dirent_count; i++) {
        const char *name = dentry->inode->dirents[i].name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        vfs_inode_t *child = vfs_core_inode_by_number(dentry->inode->dirents[i].inode_number);
        if (child && child->mode != VFS_TYPE_DIR) {
            size += child->size;
        }
    }
    return size;
}

void vfs_tree(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    kprintf("[VFS] Filesystem tree with details:\n");
    kprintf("================================================================\n");
    vfs_print_tree_recursive(vfs_core_root(), 0);
    kprintf("================================================================\n");
}

u32 vfs_mkdir(const char *path, u8 force) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char norm_path[VFS_MAX_PATH];
    path_normalize(path, norm_path, sizeof(norm_path));
    if (!vfs_core_create_dir(norm_path, force)) {
        kprintf("[VFS] ERROR: Could not create directory: %s\n", norm_path);
        return 0;
    }
    kprintf("[VFS] Created directory: %s\n", norm_path);
    return 1;
}

u32 vfs_create(const char *path, u8 force) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char norm_path[VFS_MAX_PATH];
    path_normalize(path, norm_path, sizeof(norm_path));
    if (!vfs_core_create_file(norm_path, force)) {
        kprintf("[VFS] ERROR: Could not create file: %s\n", norm_path);
        return 0;
    }
    kprintf("[VFS] Created file: %s\n", norm_path);
    return 1;
}

u32 vfs_rmdir(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char norm_path[VFS_MAX_PATH];
    path_normalize(path, norm_path, sizeof(norm_path));
    if (!vfs_core_rmdir(norm_path)) {
        kprintf("[VFS] ERROR: Unable to remove directory: %s\n", norm_path);
        return 0;
    }
    kprintf("[VFS] Removed directory: %s\n", norm_path);
    return 1;
}

/* File descriptor table */
vfs_fd_t fd_table[MAX_FDS];

/* Mount filesystem */
i32 vfs_mount(const char *mountpoint, u32 device) {
    (void)mountpoint; (void)device;
    kprintf("[VFS] Mount not implemented yet\n");
    return -1;
}

/* Unmount filesystem */
i32 vfs_unmount(const char *mountpoint) {
    (void)mountpoint;
    kprintf("[VFS] Unmount not implemented yet\n");
    return -1;
}

/* File descriptor operations */
u32 vfs_open(const char *path) {
    /* Find free fd */
    for (u32 fd = 0; fd < MAX_FDS; fd++) {
        if (fd_table[fd].inode == 0) {
            /* For now, just use RAM filesystem */
            vfs_entry_t *entry = vfs_find(path);
            if (!entry) return VFS_INVALID_FD;

            fd_table[fd].inode = 1;  /* Dummy inode */
            fd_table[fd].offset = 0;
            fd_table[fd].flags = 0;
            return fd;
        }
    }
    return VFS_INVALID_FD;
}

u32 vfs_close(u32 fd) {
    if (fd >= MAX_FDS) return 0;
    fd_table[fd].inode = 0;
    return 1;
}

u32 vfs_write(u32 fd, const void *buffer, u32 size) {
    (void)fd; (void)buffer; (void)size;
    kprintf("[VFS] Write not implemented\n");
    return 0;
}