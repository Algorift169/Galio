/* vfs_wrapper.c - VFS public API wrappers for the new core engine */
#include "vfs.h"
#include "vfs_core.h"
#include "ext2.h"
#include "heap.h"
#include "kprintf.h"
#include "pit.h"
#include "string.h"

extern u32 block_size;  /* Add this line - defined in ext2.c */

vfs_header_t *vfs_root = NULL;
static vfs_entry_t compat_entries[32];
static u32 compat_index = 0;

static char *path_normalize(const char *path) {
    static char buf[VFS_MAX_PATH];
    u32 i = 0, j = 0;

    if (!path) {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    while (path[i] && j < VFS_MAX_PATH - 1) {
        if (path[i] == '/' && i > 0 && path[i - 1] == '/') {
            i++;
            continue;
        }
        buf[j++] = path[i++];
    }
    buf[j] = 0;

    if (j > 1 && buf[j - 1] == '/') {
        buf[j - 1] = 0;
    }

    if (j == 0) {
        buf[0] = '/';
        buf[1] = 0;
    }

    return buf;
}

static vfs_entry_t *build_compat_entry(vfs_dentry_t *dentry, const char *path) {
    if (!dentry || !dentry->inode) return NULL;
    vfs_entry_t *entry = &compat_entries[compat_index++ % 32];
    char normalized[VFS_MAX_PATH];
    if (path && *path) {
        strncpy(normalized, path_normalize(path), VFS_MAX_PATH - 1);
        normalized[VFS_MAX_PATH - 1] = 0;
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

static vfs_entry_t *build_compat_entry_for_disk(u32 inode_num, const char *path) {
    if (inode_num == 0 || !path) return NULL;
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return NULL;

    vfs_entry_t *entry = &compat_entries[compat_index++ % 32];
    char normalized[VFS_MAX_PATH];
    strncpy(normalized, path_normalize(path), VFS_MAX_PATH - 1);
    normalized[VFS_MAX_PATH - 1] = 0;
    strncpy(entry->path, normalized, VFS_MAX_PATH - 1);
    entry->path[VFS_MAX_PATH - 1] = 0;
    entry->size = inode.size;
    entry->is_dir = (inode.mode & 0x4000) ? 1 : 0;
    entry->offset = inode.block[0];
    entry->permissions = entry->is_dir ? 0755 : 0644;
    return entry;
}

vfs_entry_t *vfs_find(const char *path) {
    if (!vfs_root || !path) return NULL;
    char *norm_path = path_normalize(path);
    if (vfs_core_is_disk_mode()) {
        u32 inode_num = ext2_find_inode(norm_path);
        return build_compat_entry_for_disk(inode_num, norm_path);
    }
    vfs_dentry_t *dentry = vfs_core_lookup(norm_path, 0);
    vfs_entry_t *result = build_compat_entry(dentry, norm_path);
    return result;
}


u32 vfs_read(const char *path, void *buffer, u32 size) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    return vfs_core_read_path(path, buffer, size);
}

static char *get_parent_dir(const char *path) {
    static char buf[VFS_MAX_PATH];
    const char *src = path;
    char *dst = buf;
    while (*src) *dst++ = *src++;
    *dst = 0;

    int i = __builtin_strlen(buf) - 1;
    while (i > 0 && buf[i] != '/') i--;

    if (i == 0) {
        buf[1] = 0;
    } else {
        buf[i] = 0;
    }

    return buf;
}

static u32 path_depth(const char *path) {
    u32 depth = 0;
    for (u32 i = 0; path[i]; i++) {
        if (path[i] == '/') depth++;
    }
    return depth;
}

const char *vfs_basename(const char *path) {
    static char buf[VFS_MAX_FILENAME];
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

static void vfs_listdir_disk(const char *path) {
    u32 inode_num = ext2_find_inode(path);
    if (inode_num == 0) {
        kprintf("[VFS] ERROR: Directory not found: %s\n", path);
        return;
    }

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) {
        kprintf("[VFS] ERROR: Cannot read directory inode: %s\n", path);
        return;
    }

    if (!(inode.mode & 0x4000)) {
        kprintf("[VFS] ERROR: Not a directory: %s\n", path);
        return;
    }

    u32 blk_size = ext2_get_block_size();
    u8 *buffer = kmalloc(blk_size);
    if (!buffer) {
        kprintf("[VFS] ERROR: Out of memory listing directory\n");
        return;
    }

    kprintf("Directory listing: %s\n", path);
    u32 dir_count = 0;
    u32 file_count = 0;
    u32 total_dir_size = 0;

    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        if (ext2_read_block(inode.block[i], buffer) != 0) continue;
        u32 offset = 0;
        while (offset < blk_size) {
            ext2_dirent_t *dent = (ext2_dirent_t *)(buffer + offset);
            if (dent->inode == 0 || dent->rec_len == 0) break;
            if (dent->name_len > 0) {
                char name[256];
                u32 name_len = dent->name_len;
                if (name_len >= sizeof(name)) {
                    name_len = sizeof(name) - 1;
                }
                memcpy(name, dent->name, name_len);
                name[name_len] = 0;
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    if (dent->file_type == 2) {
                        kprintf("    [DIR]  %s/\n", name);
                        dir_count++;
                    } else {
                        ext2_inode_t child_inode;
                        u32 child_size = 0;
                        if (ext2_read_inode(dent->inode, &child_inode) == 0) {
                            child_size = child_inode.size;
                        }
                        kprintf("    [FILE] %s (%u bytes)\n", name, child_size);
                        file_count++;
                        total_dir_size += child_size;
                    }
                }
            }
            offset += dent->rec_len;
        }
    }
    kfree(buffer);

    kprintf("    ─────────────────────────────────────────────\n");
    kprintf("    Dirs: %u | Files: %u | Total size: %u bytes\n",
            dir_count, file_count, total_dir_size);
}

void vfs_listdir(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    
    char *norm_path = path_normalize(path);
    
    /* Handle disk mode separately using EXT2 directly */
    if (vfs_core_is_disk_mode()) {
        u32 dir_inode = ext2_find_inode(norm_path);
        if (dir_inode == 0) {
            kprintf("[VFS] ERROR: Directory not found: %s\n", path);
            return;
        }
        
        ext2_inode_t inode;
        if (ext2_read_inode(dir_inode, &inode) != 0) {
            kprintf("[VFS] ERROR: Cannot read directory inode: %s\n", path);
            return;
        }
        
        if (!(inode.mode & 0x4000)) {
            kprintf("[VFS] ERROR: Not a directory: %s\n", path);
            return;
        }
        
        kprintf("Directory listing: %s\n", norm_path);
        u32 dir_count = 0;
        u32 file_count = 0;
        u32 total_dir_size = 0;
        
        /* Read directory blocks */
        u32 blk_size = ext2_get_block_size();
        u8 *buffer = kmalloc(blk_size);
        if (!buffer) {
            kprintf("[VFS] ERROR: Out of memory listing directory\n");
            return;
        }
        for (u32 i = 0; i < 12 && inode.block[i]; i++) {
            if (ext2_read_block(inode.block[i], buffer) != 0) continue;
            
            ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
            while ((u8 *)dent < buffer + block_size) {
                u8 *dent_ptr = (u8 *)dent;
                u32 remaining = (u32)((buffer + block_size) - dent_ptr);

                if (dent->rec_len == 0 || dent->rec_len > remaining) break;

                if (dent->inode == 0 || dent->name_len == 0) {
                    if (dent_ptr + dent->rec_len > buffer + block_size) break;
                    dent = (ext2_dirent_t *)(dent_ptr + dent->rec_len);
                    continue;
                }

                char name[256];
                u32 name_len = dent->name_len;
                if (name_len > 255) name_len = 255;
                memcpy(name, dent->name, name_len);
                name[name_len] = 0;
                
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    ext2_inode_t child_inode;
                    if (ext2_read_inode(dent->inode, &child_inode) == 0) {
                        if (child_inode.mode & 0x4000) {
                            kprintf("    [DIR]  %s/\n", name);
                            dir_count++;
                        } else {
                            kprintf("    [FILE] %s (%u bytes)\n", name, child_inode.size);
                            file_count++;
                            total_dir_size += child_inode.size;
                        }
                    }
                }
                if (dent_ptr + dent->rec_len > buffer + block_size) break;
                dent = (ext2_dirent_t *)(dent_ptr + dent->rec_len);
            }
        }
        kfree(buffer);
        kprintf("    ─────────────────────────────────────────────\n");
        kprintf("    Dirs: %u | Files: %u | Total size: %u bytes\n",
                dir_count, file_count, total_dir_size);
        return;
    }
    
    /* RAM mode - original code */
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
        if (strcmp(dent->name, ".") == 0 || strcmp(dent->name, "..") == 0) continue;
        vfs_inode_t *child = vfs_core_inode_by_number(dent->inode_number);
        if (!child) continue;
        if (child->mode == VFS_TYPE_DIR) {
            kprintf("    [DIR]  %s/\n", dent->name);
            dir_count++;
        } else {
            kprintf("    [FILE] %s (%u bytes)\n", dent->name, child->size);
            file_count++;
            total_dir_size += child->size;
        }
    }
    
    kprintf("    ─────────────────────────────────────────────\n");
    kprintf("    Dirs: %u | Files: %u | Total size: %u bytes\n",
            dir_count, file_count, total_dir_size);
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

static void vfs_stats_recurse(vfs_dentry_t *node, u32 *dir_count, u32 *file_count, u32 *data_size) {
    if (!node || !node->inode) return;
    if (node != vfs_core_root()) {
        if (node->inode->mode == VFS_TYPE_DIR) {
            (*dir_count)++;
        } else {
            (*file_count)++;
            *data_size += node->inode->size;
        }
    }
    for (vfs_dentry_t *child = node->first_child; child; child = child->next_sibling) {
        vfs_stats_recurse(child, dir_count, file_count, data_size);
    }
}

static void vfs_stats_disk_recursive(const char *current_path, u32 *dirs, u32 *files, u32 *total_data_size) {
    u32 inode_num = ext2_find_inode(current_path);
    if (inode_num == 0) return;

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return;
    if (!(inode.mode & 0x4000)) return;

    u32 blk_size = ext2_get_block_size();
    u8 *buffer = kmalloc(blk_size);
    if (!buffer) return;

    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        if (ext2_read_block(inode.block[i], buffer) != 0) continue;
        u32 offset = 0;
        while (offset < blk_size) {
            ext2_dirent_t *dent = (ext2_dirent_t *)(buffer + offset);
            if (dent->inode == 0 || dent->rec_len == 0) break;
            if (dent->name_len > 0) {
                char name[256];
                u32 name_len = dent->name_len;
                if (name_len >= sizeof(name)) {
                    name_len = sizeof(name) - 1;
                }
                memcpy(name, dent->name, name_len);
                name[name_len] = 0;
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    ext2_inode_t child_inode;
                    if (ext2_read_inode(dent->inode, &child_inode) == 0) {
                        if (child_inode.mode & 0x4000) {
                            (*dirs)++;
                            char next_path[VFS_MAX_PATH];
                            next_path[0] = 0;
                            if (strcmp(current_path, "/") == 0) {
                                strncpy(next_path, "/", VFS_MAX_PATH - 1);
                                next_path[VFS_MAX_PATH - 1] = 0;
                            } else {
                                strncpy(next_path, current_path, VFS_MAX_PATH - 1);
                                next_path[VFS_MAX_PATH - 1] = 0;
                            }
                            if (strlen(next_path) + 1 + name_len < VFS_MAX_PATH) {
                                if (strcmp(next_path, "/") != 0) {
                                    strncat(next_path, "/", VFS_MAX_PATH - strlen(next_path) - 1);
                                }
                                strncat(next_path, name, VFS_MAX_PATH - strlen(next_path) - 1);
                                vfs_stats_disk_recursive(next_path, dirs, files, total_data_size);
                            }
                        } else {
                            (*files)++;
                            *total_data_size += child_inode.size;
                        }
                    }
                }
            }
            offset += dent->rec_len;
        }
    }
    kfree(buffer);
}

void vfs_stats(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return;
    }
    if (vfs_core_is_disk_mode()) {
        u32 dirs = 0, files = 0, total_data_size = 0;
        vfs_stats_disk_recursive("/", &dirs, &files, &total_data_size);
        kprintf("[VFS] Filesystem Statistics (disk mode):\n");
        kprintf("    Total entries:    %u\n", dirs + files);
        kprintf("    Directories:      %u\n", dirs);
        kprintf("    Files:            %u\n", files);
        kprintf("    Total data size:  %u bytes (%.2f KB)\n",
                total_data_size, (float)total_data_size / 1024.0);
        kprintf("    Mode:             disk-backed EXT2\n");
        return;
    }
    u32 dirs = 0, files = 0, total_data_size = 0;
    vfs_stats_recurse(vfs_core_root(), &dirs, &files, &total_data_size);
    kprintf("[VFS] Filesystem Statistics:\n");
    kprintf("    Total entries:    %u\n", dirs + files);
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
    if (vfs_core_is_disk_mode()) {
        return;
    }
    vfs_dentry_t *root = vfs_core_root();
    if (!root) {
        kprintf("[VFS] ERROR: Root directory unavailable\n");
        return;
    }
    kprintf("╔════════════════════════════════════════════════════════════════╗\n");
    kprintf("║        FILESYSTEM TREE (RAM-BACKED, WRITABLE)                  ║\n");
    kprintf("╠════════════════════════════════════════════════════════════════╣\n");
    vfs_print_tree_recursive(root, 0);
    kprintf("╚════════════════════════════════════════════════════════════════╝\n");
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
    char *norm_path = path_normalize(path);
    if (!vfs_core_create_dir(norm_path, force)) {
        kprintf("[VFS] ERROR: Could not create directory: %s\n", norm_path);
        return 0;
    }
    return 1;
}

u32 vfs_create(const char *path, u8 force) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char *norm_path = path_normalize(path);
    if (!vfs_core_create_file(norm_path, force)) {
        kprintf("[VFS] ERROR: Could not create file: %s\n", norm_path);
        return 0;
    }
    return 1;
}

u32 vfs_rmdir(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char *norm_path = path_normalize(path);
    if (!vfs_core_rmdir(norm_path)) {
        kprintf("[VFS] ERROR: Unable to remove directory: %s\n", norm_path);
        return 0;
    }
    return 1;
}

u32 vfs_move(const char *source, const char *target) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    char src_norm[VFS_MAX_PATH];
    char dst_norm[VFS_MAX_PATH];
    strncpy(src_norm, path_normalize(source), VFS_MAX_PATH - 1);
    src_norm[VFS_MAX_PATH - 1] = 0;
    strncpy(dst_norm, path_normalize(target), VFS_MAX_PATH - 1);
    dst_norm[VFS_MAX_PATH - 1] = 0;
    return vfs_core_move(src_norm, dst_norm);
}

u32 vfs_open(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return VFS_INVALID_FD;
    }
    u32 fd = vfs_core_open(path);
    if (fd == VFS_INVALID_FD) {
        kprintf("[VFS] ERROR: Could not open file: %s\n", path);
    }
    return fd;
}

u32 vfs_close(u32 fd) {
    return vfs_core_close(fd);
}

u32 vfs_write(u32 fd, const void *buffer, u32 size) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    return vfs_core_write(fd, buffer, size);
}

u32 vfs_unlink(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    if (!vfs_core_unlink(path)) {
        kprintf("[VFS] ERROR: Could not unlink: %s\n", path);
        return 0;
    }
    return 1;
}

static u8 vfs_disk_entry_is_dot(const ext2_dirent_t *dent) {
    return (dent->name_len == 1 && dent->name[0] == '.') ||
           (dent->name_len == 2 && dent->name[0] == '.' && dent->name[1] == '.');
}

static void vfs_build_disk_child_path(const char *parent, const ext2_dirent_t *dent, char *out_path) {
    if (strcmp(parent, "/") == 0) {
        strncpy(out_path, "/", VFS_MAX_PATH - 1);
        out_path[VFS_MAX_PATH - 1] = 0;
        strncat(out_path, dent->name, dent->name_len);
        out_path[VFS_MAX_PATH - 1] = 0;
        return;
    }

    strncpy(out_path, parent, VFS_MAX_PATH - 1);
    out_path[VFS_MAX_PATH - 1] = 0;
    if (out_path[strlen(out_path) - 1] != '/') {
        strncat(out_path, "/", VFS_MAX_PATH - strlen(out_path) - 1);
    }
    strncat(out_path, dent->name, dent->name_len);
    out_path[VFS_MAX_PATH - 1] = 0;
}

static u32 vfs_remove_recursive_disk(const char *path) {
    if (!vfs_core_is_disk_mode()) return 0;
    if (!path || strcmp(path, "/") == 0) return 0;

    u32 inode_num = ext2_find_inode(path);
    if (inode_num == 0) return 0;

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return 0;

    if (!(inode.mode & 0x4000)) {
        return ext2_unlink(path) == 0 ? 1 : 0;
    }

    u8 buffer[block_size];
    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        if (ext2_read_block(inode.block[i], buffer) != 0) continue;
        ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
        while ((u8 *)dent < buffer + block_size) {
            if (dent->rec_len == 0 || dent->rec_len > block_size) break;
            if (dent->inode && !vfs_disk_entry_is_dot(dent)) {
                char child_path[VFS_MAX_PATH];
                vfs_build_disk_child_path(path, dent, child_path);
                if (!vfs_remove_recursive_disk(child_path)) return 0;
            }
            dent = (ext2_dirent_t *)((u8 *)dent + dent->rec_len);
        }
    }

    return ext2_rmdir(path) == 0 ? 1 : 0;
}

static u32 vfs_remove_recursive_dentry(vfs_dentry_t *dentry) {
    if (!dentry || !dentry->inode) return 0;
    if (dentry == vfs_core_root()) return 0;

    if (vfs_core_is_disk_mode()) {
        char fullpath[VFS_MAX_PATH];
        vfs_core_build_path(dentry, fullpath);
        return vfs_remove_recursive_disk(fullpath);
    }

    if (dentry->inode->mode == VFS_TYPE_DIR) {
        vfs_dentry_t *child = dentry->first_child;
        while (child) {
            vfs_dentry_t *next = child->next_sibling;
            if (strcmp(child->name, ".") != 0 && strcmp(child->name, "..") != 0) {
                if (!vfs_remove_recursive_dentry(child)) return 0;
            }
            child = next;
        }
        char fullpath[VFS_MAX_PATH];
        vfs_core_build_path(dentry, fullpath);
        return vfs_core_rmdir(fullpath);
    }

    char fullpath[VFS_MAX_PATH];
    vfs_core_build_path(dentry, fullpath);
    return vfs_core_unlink(fullpath);
}

static u32 vfs_cleanup_old_entries(vfs_dentry_t *dentry, u32 age_ticks, u32 current_ticks) {
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    u32 removed = 0;
    vfs_dentry_t *child = dentry->first_child;
    while (child) {
        vfs_dentry_t *next = child->next_sibling;
        if (strcmp(child->name, ".") != 0 && strcmp(child->name, "..") != 0) {
            if (child->inode->mode == VFS_TYPE_DIR) {
                if (child->inode->ctime != 0 && current_ticks - child->inode->ctime >= age_ticks) {
                    char fullpath[VFS_MAX_PATH];
                    vfs_core_build_path(child, fullpath);
                    if (vfs_remove_recursive_dentry(child)) {
                        removed++;
                    }
                } else {
                    removed += vfs_cleanup_old_entries(child, age_ticks, current_ticks);
                }
            } else {
                if (child->inode->ctime != 0 && current_ticks - child->inode->ctime >= age_ticks) {
                    char fullpath[VFS_MAX_PATH];
                    vfs_core_build_path(child, fullpath);
                    if (vfs_core_unlink(fullpath)) {
                        removed++;
                    }
                }
            }
        }
        child = next;
    }
    return removed;
}

u32 vfs_remove_recursive(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) return 0;
    if (dentry == vfs_core_root()) return 0;
    if (!vfs_remove_recursive_dentry(dentry)) {
        kprintf("[VFS] ERROR: Could not remove recursively: %s\n", path);
        return 0;
    }
    return 1;
}

u32 vfs_remove_dir_contents(const char *path) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    if (vfs_core_is_disk_mode()) {
        if (!path || strcmp(path, "/") == 0) {
            kprintf("[VFS] ERROR: Cannot clear root directory contents directly\n");
            return 0;
        }
        u32 inode_num = ext2_find_inode(path);
        if (inode_num == 0) {
            kprintf("[VFS] ERROR: Directory not found: %s\n", path);
            return 0;
        }
        ext2_inode_t inode;
        if (ext2_read_inode(inode_num, &inode) != 0) {
            kprintf("[VFS] ERROR: Could not read directory inode: %s\n", path);
            return 0;
        }
        u32 removed = 0;
        u8 success = 1;
        u8 buffer[block_size];
        for (u32 i = 0; i < 12 && inode.block[i]; i++) {
            if (ext2_read_block(inode.block[i], buffer) != 0) {
                success = 0;
                continue;
            }
            ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
            while ((u8 *)dent < buffer + block_size) {
                if (dent->rec_len == 0 || dent->rec_len > block_size) break;
                if (dent->inode && !vfs_disk_entry_is_dot(dent)) {
                    char child_path[VFS_MAX_PATH];
                    vfs_build_disk_child_path(path, dent, child_path);
                    if (vfs_remove_recursive_disk(child_path)) {
                        removed++;
                    } else {
                        kprintf("[VFS] ERROR: Could not remove entry: %.*s\n", dent->name_len, dent->name);
                        success = 0;
                    }
                }
                /* bounds safe advance */
                u8 *dent_ptr = (u8 *)dent;
                if (dent_ptr + dent->rec_len > buffer + block_size) break;
                dent = (ext2_dirent_t *)(dent_ptr + dent->rec_len);
            }
        }
        return success;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) {
        kprintf("[VFS] ERROR: Directory not found: %s\n", path);
        return 0;
    }
    u32 removed = 0;
    u8 success = 1;
    vfs_dentry_t *child = dentry->first_child;
    while (child) {
        vfs_dentry_t *next = child->next_sibling;
        if (strcmp(child->name, ".") != 0 && strcmp(child->name, "..") != 0) {
            char fullpath[VFS_MAX_PATH];
            vfs_core_build_path(child, fullpath);
            if (vfs_remove_recursive_dentry(child)) {
                removed++;
            } else {
                kprintf("[VFS] ERROR: Could not remove entry: %s\n", fullpath);
                success = 0;
            }
        }
        child = next;
    }
    return success;
}

u32 vfs_cleanup_old_recycle_bin(const char *path, u32 age_ticks) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_DIR) return 0;
    u32 current_ticks = pit_get_ticks();
    return vfs_cleanup_old_entries(dentry, age_ticks, current_ticks);
}

/* Read from an open file descriptor */
u32 vfs_read_fd(u32 fd, void *buffer, u32 size) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    return vfs_core_read(fd, buffer, size);
}

/* Seek within an open file */
u32 vfs_lseek(u32 fd, i32 offset, i32 whence) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return (u32)-1;
    }
    return vfs_core_lseek(fd, offset, whence);
}

/* Get file stat information */
u32 vfs_stat(const char *path, void *statbuf) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    return vfs_core_stat(path, statbuf);
}

/* Force all filesystem data to disk - ensures persistence */
u32 vfs_fsync(void) {
    if (!vfs_root) {
        kprintf("[VFS] ERROR: Filesystem not mounted\n");
        return 0;
    }
    if (vfs_core_is_disk_mode()) {
        if (ext2_fsync() == 0) {
            return 1;
        }
        kprintf("[VFS] ERROR: Filesystem sync failed\n");
        return 0;
    }
    kprintf("[VFS] RAM mode: no disk sync needed\n");
    return 1;
}