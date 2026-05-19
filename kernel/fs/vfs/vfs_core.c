#include "vfs_core.h"
#include "heap.h"
#include "kprintf.h"
#include "string.h"
#include "ext2.h"

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
            return vfs_inodes[i].mode == VFS_TYPE_DIR;
        }
    }
    return 0;
}

/* Helper functions for RAM mode */
static void vfs_reset_ram(void) {
    if (vfs_data_ram) { kfree(vfs_data_ram); vfs_data_ram = NULL; }
    vfs_data_ram_size = 0; vfs_data_top = 0;
}
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
        vfs_files[i].inode = NULL; vfs_files[i].pos = 0; vfs_files[i].flags = 0; vfs_files[i].ref_count = 0;
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
    if (!entry || !entry->name) return;
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
    for (u32 i = 0; i < VFS_MAX_BLOCKS; i++) inode->blocks[i] = 0;
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
    if (dentry == vfs_root_dentry) { buffer[0] = '/'; buffer[1] = 0; return; }
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
    if (temp[0] == 0) { buffer[0] = '/'; buffer[1] = 0; }
    else { strncpy(buffer, temp, VFS_MAX_PATH-1); buffer[VFS_MAX_PATH-1] = 0; }
}
static char *vfs_normalize(const char *path, char *buffer) {
    if (!path || !buffer) return NULL;
    u32 di = 0, i = 0;
    while (path[i] && di+1 < VFS_MAX_PATH) {
        if (path[i] == '/' && i>0 && path[i-1]=='/') { i++; continue; }
        buffer[di++] = path[i++];
    }
    buffer[di] = 0;
    if (di>1 && buffer[di-1]=='/') buffer[--di] = 0;
    if (di==0) { buffer[0]='/'; buffer[1]=0; }
    return buffer;
}
static void vfs_split_parent(const char *path, char *parent, char *name) {
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    if (strcmp(normalized,"/")==0) { strcpy(parent,"/"); name[0]=0; return; }
    const char *slash = NULL;
    for (const char *p = normalized; *p; p++) if (*p=='/') slash = p;
    if (!slash || slash==normalized) {
        strcpy(parent,"/"); strcpy(name, normalized+1); return;
    }
    u32 parent_len = slash - normalized;
    if (parent_len >= VFS_MAX_PATH) parent_len = VFS_MAX_PATH-1;
    memcpy(parent, normalized, parent_len); parent[parent_len]=0;
    strcpy(name, slash+1);
}
static vfs_dentry_t *vfs_lookup_internal(const char *path) {
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    if (strcmp(normalized,"/")==0) return vfs_root_dentry;
    vfs_dentry_t *current = vfs_root_dentry;
    const char *cursor = normalized + 1;
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
    if (strcmp(normalized,"/")==0) return vfs_root_dentry;
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
        if (!parent || !parent->inode || parent->inode->mode != VFS_TYPE_DIR) return NULL;
        if (vfs_find_dirent(parent->inode, name)) {
            vfs_inode_t *existing = vfs_find_inode_in_dir(parent->inode, name);
            if (existing && existing->mode == VFS_TYPE_DIR) return parent;
            return NULL;
        }
        vfs_inode_t *inode = vfs_alloc_inode();
        if (!inode) return NULL;
        inode->mode = VFS_TYPE_DIR;
        inode->size = 0; inode->link_count = 1; inode->dirent_count = 0;
        inode->dirent_capacity = 0; inode->dirents = NULL;
        vfs_dirent_add(inode, ".", inode->number);
        vfs_dirent_add(inode, "..", parent->inode->number);
        if (!vfs_dirent_add(parent->inode, name, inode->number)) return NULL;
        vfs_dentry_t *child = vfs_create_dentry(parent, name, inode);
        return child;
    }
}
static vfs_dentry_t *vfs_make_file_internal(const char *path, u8 force, const u8 *data, u32 size) {
    if (!path) return NULL;
    char normalized[VFS_MAX_PATH];
    vfs_normalize(path, normalized);
    char parent_path[VFS_MAX_PATH], name[VFS_MAX_FILENAME];
    vfs_split_parent(normalized, parent_path, name);
    if (vfs_disk_mode) {
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
    } else {
        vfs_dentry_t *parent = vfs_lookup_internal(parent_path);
        if (!parent || !parent->inode || parent->inode->mode != VFS_TYPE_DIR) return NULL;
        vfs_inode_t *existing = vfs_find_inode_in_dir(parent->inode, name);
        if (existing) {
            if (existing->mode == VFS_TYPE_DIR) return NULL;
            if (!force) return NULL;
            u32 new_offset = vfs_allocate_data(size);
            if (new_offset == 0xFFFFFFFFu) return NULL;
            if (data && size) memcpy(vfs_data_ram + new_offset, data, size);
            existing->blocks[0] = new_offset; existing->block_count = 1; existing->size = size;
            return vfs_cache_lookup(parent, name);
        }
        vfs_inode_t *inode = vfs_alloc_inode();
        if (!inode) return NULL;
        inode->mode = VFS_TYPE_FILE;
        inode->size = size; inode->link_count = 1;
        u32 data_offset = 0;
        if (size > 0) {
            data_offset = vfs_allocate_data(size);
            if (data_offset == 0xFFFFFFFFu) return NULL;
            if (data) memcpy(vfs_data_ram + data_offset, data, size);
            else memset(vfs_data_ram + data_offset, 0, size);
            inode->blocks[0] = data_offset; inode->block_count = 1;
        }
        if (!vfs_dirent_add(parent->inode, name, inode->number)) return NULL;
        vfs_dentry_t *child = vfs_create_dentry(parent, name, inode);
        return child;
    }
}
static void vfs_init_root(void) {
    vfs_root_inode = vfs_alloc_inode();
    if (!vfs_root_inode) return;
    vfs_root_inode->mode = VFS_TYPE_DIR;
    vfs_root_inode->size = 0; vfs_root_inode->link_count = 1;
    vfs_root_inode->dirent_count = 0; vfs_root_inode->dirent_capacity = 0; vfs_root_inode->dirents = NULL;
    vfs_root_dentry = vfs_create_dentry(NULL, "", vfs_root_inode);
    if (vfs_root_dentry) vfs_root_dentry->parent = vfs_root_dentry;
    vfs_dirent_add(vfs_root_inode, ".", vfs_root_inode->number);
    vfs_dirent_add(vfs_root_inode, "..", vfs_root_inode->number);
}
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
        if (parent[0] != 0 && strcmp(parent,"/")!=0) vfs_make_directory_internal(parent, 1);
        u32 size = entry->size;
        u8 *source = (u8 *)header + entry->offset;
        vfs_make_file_internal(entry->path, 1, source, size);
    }
}
void vfs_core_init(void *initrd_addr) {
    if (!initrd_addr) { kprintf("[VFS] ERROR: No initrd address supplied\n"); return; }
    vfs_header_t *header = (vfs_header_t *)initrd_addr;
    if (header->magic != VFS_MAGIC) { kprintf("[VFS] ERROR: Invalid initrd magic %08X\n", header->magic); return; }
    vfs_reset_state();
    vfs_init_root();
    if (!vfs_root_dentry) { kprintf("[VFS] ERROR: Root directory initialization failed\n"); return; }
    vfs_build_from_initrd(header);
    kprintf("[VFS] Core filesystem initialized in RAM\n");
}
vfs_dentry_t *vfs_core_lookup(const char *path, u32 flags) {
    if (!path) return NULL;
    if (flags & VFS_LOOKUP_PARENT) return vfs_lookup_parent_internal(path);
    return vfs_lookup_internal(path);
}
vfs_dentry_t *vfs_core_root(void) { return vfs_root_dentry; }
void vfs_core_build_path(vfs_dentry_t *dentry, char *buffer) { vfs_build_path_from_dentry(dentry, buffer); }

u32 vfs_core_open(const char *path) {
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_FILE) return VFS_INVALID_FD;
    for (u32 i = 0; i < VFS_MAX_FILE_HANDLES; i++) {
        if (vfs_files[i].ref_count == 0) {
            vfs_files[i].inode = dentry->inode;
            vfs_files[i].pos = 0; vfs_files[i].flags = 0; vfs_files[i].ref_count = 1;
            return i;
        }
    }
    return VFS_INVALID_FD;
}
u32 vfs_core_close(u32 fd) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    if (vfs_files[fd].ref_count == 0) return 0;
    vfs_files[fd].ref_count = 0; vfs_files[fd].inode = NULL; vfs_files[fd].pos = 0; vfs_files[fd].flags = 0;
    return 1;
}
u32 vfs_core_write(u32 fd, const void *buffer, u32 size) {
    if (fd >= VFS_MAX_FILE_HANDLES) return 0;
    vfs_file_t *fh = &vfs_files[fd];
    if (!fh->inode || fh->ref_count == 0 || fh->inode->mode != VFS_TYPE_FILE) return 0;
    if (vfs_disk_mode) {
        i32 written = ext2_write_data(fh->inode->number, buffer, size);
        if (written < 0) return 0;
        fh->inode->size = written;
        fh->pos = written;
        return written;
    } else {
        if (size > 0) {
            u32 offset = vfs_allocate_data(size);
            if (offset == 0xFFFFFFFFu) return 0;
            memcpy(vfs_data_ram + offset, buffer, size);
            fh->inode->blocks[0] = offset; fh->inode->block_count = 1;
        }
        fh->inode->size = size; fh->pos = size;
        return size;
    }
}
u32 vfs_core_read_path(const char *path, void *buffer, u32 size) {
    if (!path || !buffer) return 0;
    if (vfs_disk_mode) {
        u32 inode_num = ext2_find_inode(path);
        if (inode_num == 0) return 0;
        i32 result = ext2_read_data(inode_num, buffer, size, 0);
        return result > 0 ? (u32)result : 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_FILE) return 0;
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
        i32 inode_num = ext2_create_directory(path, 0x41ED);
        return inode_num > 0 ? 1 : 0;
    }
    vfs_dentry_t *created = vfs_make_directory_internal(path, force);
    return created != NULL;
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
    if (!dentry || !dentry->inode || dentry->inode->mode != VFS_TYPE_FILE) return 0;
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
    if (!fh->inode || fh->ref_count == 0 || fh->inode->mode != VFS_TYPE_FILE) return 0;
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