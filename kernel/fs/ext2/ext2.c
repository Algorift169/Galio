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

/* ext2.c - Simplified EXT2 filesystem driver with working writes */
#include "ext2.h"
#include "ata.h"
#include "heap.h"
#include "kprintf.h"
#include "string.h"

#define EXT2_ROOT_INODE 2

static ext2_superblock_t superblock;
static ext2_group_desc_t *group_descs = NULL;
static u32 ext2_group_count = 0;
static u32 ext2_group_desc_block = 0;
static u32 ext2_group_desc_blocks = 0;
u32 block_size = EXT2_BLOCK_SIZE;   /* exported for VFS */
static u32 inodes_per_block;
static u32 blocks_per_group;
static u32 ext2_partition_lba = 0; /* LBA of the start of the EXT2 partition */

#define EXT2_BLOCK_CACHE_SLOTS 16

typedef struct {
    u32 block_num;
    u8 valid;
    u8 data[4096];
} ext2_block_cache_entry_t;

static ext2_block_cache_entry_t ext2_block_cache[EXT2_BLOCK_CACHE_SLOTS];
static u32 ext2_block_cache_next = 0;

static void ext2_cache_init(void) {
    for (u32 i = 0; i < EXT2_BLOCK_CACHE_SLOTS; i++) {
        ext2_block_cache[i].block_num = 0xFFFFFFFFu;
        ext2_block_cache[i].valid = 0;
    }
    ext2_block_cache_next = 0;
}

static i32 ext2_cache_find(u32 block_num) {
    for (u32 i = 0; i < EXT2_BLOCK_CACHE_SLOTS; i++) {
        if (ext2_block_cache[i].valid && ext2_block_cache[i].block_num == block_num) {
            return (i32)i;
        }
    }
    return -1;
}

static i32 ext2_cache_store(u32 block_num, const void *buffer) {
    u32 slot = ext2_cache_find(block_num);
    if (slot == (u32)-1) {
        slot = ext2_block_cache_next++ % EXT2_BLOCK_CACHE_SLOTS;
    }
    ext2_block_cache[slot].block_num = block_num;
    ext2_block_cache[slot].valid = 1;
    memcpy(ext2_block_cache[slot].data, buffer, block_size);
    return 0;
}

static u32 ext2_block_to_lba(u32 block_num);
static const char *ext2_canonicalize_path(const char *path, char *buffer, u32 buffer_size);

static i32 ext2_cache_read_block(u32 block_num, void *buffer) {
    i32 slot = ext2_cache_find(block_num);
    if (slot >= 0) {
        memcpy(buffer, ext2_block_cache[slot].data, block_size);
        return 0;
    }

    u32 sector = ext2_block_to_lba(block_num);
    u32 sectors = block_size / 512;
    if (ata_read_sectors(sector, sectors, buffer) < 0) return -1;
    ext2_cache_store(block_num, buffer);
    return 0;
}

static i32 ext2_cache_write_block(u32 block_num, const void *buffer) {
    u32 sector = ext2_block_to_lba(block_num);
    u32 sectors = block_size / 512;
    if (ata_write_sectors(sector, sectors, buffer) < 0) return -1;
    ext2_cache_store(block_num, buffer);
    return 0;
}

static const char *ext2_canonicalize_path(const char *path, char *buffer, u32 buffer_size) {
    if (!path || !buffer || buffer_size == 0) return NULL;

    const char *src = path;
    if (path[0] == '.') {
        if (path[1] == '\0') {
            if (buffer_size < 2) return NULL;
            buffer[0] = '/';
            buffer[1] = '\0';
            return buffer;
        }
        if (path[1] == '/') {
            src = path + 2;
        } else {
            return NULL;
        }
    } else if (path[0] == '/') {
        src = path + 1;
    } else {
        return NULL;
    }

    u32 pos = 0;
    if (buffer_size < 2) return NULL;
    buffer[pos++] = '/';

    u32 i = 0;
    while (src[i] != '\0') {
        while (src[i] == '/') i++;
        if (src[i] == '\0') break;
        if (pos + 1 >= buffer_size) return NULL;

        const char *start = src + i;
        u32 comp_len = 0;
        while (src[i] != '\0' && src[i] != '/') {
            if (comp_len + 1 < buffer_size) {
                comp_len++;
            }
            i++;
        }

        if (comp_len == 0) continue;

        if (src[start - src] == '.' && comp_len == 1 && start[0] == '.') {
            if (pos > 1) {
                while (pos > 1 && buffer[pos - 1] != '/') pos--; // pop last component
            }
            continue;
        }

        if (src[start - src] == '.' && comp_len == 2 && start[0] == '.' && start[1] == '.') {
            if (pos > 1) {
                while (pos > 1 && buffer[pos - 1] != '/') pos--; // pop last component
            }
            continue;
        }

        if (buffer[pos - 1] != '/') {
            if (pos + 1 >= buffer_size) return NULL;
            buffer[pos++] = '/';
        }
        if (pos + comp_len >= buffer_size) return NULL;
        memcpy(buffer + pos, start, comp_len);
        pos += comp_len;
    }

    if (pos == 1) {
        buffer[1] = '\0';
    } else {
        buffer[pos] = '\0';
    }
    return buffer;
}

/* Set the filesystem base LBA on disk (partition start) */
void ext2_set_partition_lba(u32 lba) {
    ext2_partition_lba = lba;
    kprintf("EXT2: partition LBA set to %u\n", ext2_partition_lba);
}

/* Convert filesystem-relative block number to absolute disk LBA */
static u32 ext2_block_to_lba(u32 block_num) {
    return ext2_partition_lba + block_num * (block_size / 512);
}

/* Read a block from disk (filesystem-relative block number) */
static i32 read_block(u32 block_num, void *buffer) {
    return ext2_cache_read_block(block_num, buffer);
}

/* Write a block to disk (filesystem-relative block number) */
static i32 write_block(u32 block_num, const void *buffer) {
    return ext2_cache_write_block(block_num, buffer);
}

static i32 ext2_write_group_descriptors(void) {
    if (!group_descs || ext2_group_desc_blocks == 0) return -1;
    for (u32 i = 0; i < ext2_group_desc_blocks; i++) {
        if (write_block(ext2_group_desc_block + i, (u8 *)group_descs + i * block_size) < 0)
            return -1;
    }
    return 0;
}

static i32 ext2_read_group_descriptors(void) {
    if (group_descs) {
        kfree(group_descs);
        group_descs = NULL;
    }
    
    ext2_group_count = (superblock.blocks_count + blocks_per_group - 1) / blocks_per_group;
    ext2_group_desc_blocks = ((ext2_group_count * sizeof(ext2_group_desc_t)) + block_size - 1) / block_size;
    ext2_group_desc_block = superblock.first_data_block + 1;
    
    group_descs = kmalloc(ext2_group_desc_blocks * block_size);
    if (!group_descs) {
        kprintf("EXT2: Unable to allocate group descriptor buffer\n");
        return -1;
    }

    for (u32 i = 0; i < ext2_group_desc_blocks; i++) {
        i32 ret = read_block(ext2_group_desc_block + i, (u8 *)group_descs + i * block_size);
        if (ret < 0) {
            kprintf("EXT2: Failed to read group descriptor block %u\n", ext2_group_desc_block + i);
            kfree(group_descs);
            group_descs = NULL;
            return -1;
        }
    }
    return 0;
}

/* Read superblock from disk image/partition start (offset 1024 bytes) */
static i32 ext2_read_superblock(void) {
    u8 buffer[1024];
    if (ata_read_sectors(ext2_partition_lba + 2, 2, buffer) < 0) return -1;
    memcpy(&superblock, buffer, sizeof(ext2_superblock_t));
    return 0;
}

/* Write superblock back to disk image/partition start */
static i32 ext2_write_superblock(void) {
    u8 buffer[1024];
    memset(buffer, 0, 1024);
    memcpy(buffer, &superblock, sizeof(ext2_superblock_t));
    /* ata_write_sectors returns count on success */
    return ata_write_sectors(ext2_partition_lba + 2, 2, buffer) < 0 ? -1 : 0;
}

static i32 ext2_initialize_root_inode(void);

i32 ext2_init(void) {
    kprintf("Initializing EXT2 filesystem...\n");

    if (ext2_read_superblock() != 0) {
        kprintf("EXT2: Failed to read superblock\n");
        return -1;
    }

    if (superblock.magic != EXT2_SIGNATURE) {
        kprintf("EXT2: Invalid signature 0x%x\n", superblock.magic);
        return -1;
    }

    block_size = 1024 << superblock.log_block_size;
    inodes_per_block = block_size / EXT2_INODE_SIZE;
    blocks_per_group = superblock.blocks_per_group;

    if (ext2_read_group_descriptors() != 0) {
        kprintf("EXT2: Failed to read group descriptors\n");
        return -1;
    }

    ext2_cache_init();

    kprintf("EXT2: %u inodes, %u blocks, block size %u, groups %u\n",
            superblock.inodes_count, superblock.blocks_count, block_size, ext2_group_count);

    /* Initialize root inode if not properly set up */
    if (ext2_initialize_root_inode() != 0) {
        kprintf("EXT2: Root inode initialization failed\n");
        return -1;
    }

    return 0;
}

/* Force sync all metadata to disk - ensures all pending writes are flushed */
i32 ext2_fsync(void) {
    /* Write superblock and group descriptors to ensure all metadata is persisted */
    if (ext2_write_superblock() != 0) {
        kprintf("[EXT2] fsync: Failed to write superblock\n");
        return -1;
    }
    
    if (ext2_write_group_descriptors() != 0) {
        kprintf("[EXT2] fsync: Failed to write group descriptors\n");
        return -1;
    }
    
    /* Flush the ATA write cache to ensure data reaches the physical disk */
    if (ata_flush_cache() != 0) {
        kprintf("[EXT2] fsync: Warning - ATA cache flush failed\n");
        return -1;
    }
    
    return 0;
}

/* Initialize root inode (inode 2) if it's not properly set up */
static i32 ext2_initialize_root_inode(void) {
    ext2_inode_t root_inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
        kprintf("EXT2: Failed to read root inode\n");
        return -1;
    }

    /* Only repair root if it has no directory data block.
       Some on-disk fields (like mode bits) may not match our simplified inode parser,
       but block[0] is the critical part for path lookups. */
    if (root_inode.block[0] == 0) {
        kprintf("EXT2: Root inode has no directory block, setting up...\n");
        
        /* Mark inode 2 as allocated in the inode bitmap if not already */
        u32 group = (EXT2_ROOT_INODE - 1) / superblock.inodes_per_group;
        u32 index = (EXT2_ROOT_INODE - 1) % superblock.inodes_per_group;
        u32 bitmap_block = group_descs[group].bg_inode_bitmap;
        u8 bitmap[block_size];
        if (read_block(bitmap_block, bitmap) == 0) {
            u32 byte_index = index / 8;
            u32 bit_index = index % 8;
            if (!(bitmap[byte_index] & (1 << bit_index))) {
                bitmap[byte_index] |= (1 << bit_index);
                if (write_block(bitmap_block, bitmap) < 0) {
                    kprintf("EXT2: Failed to mark root inode in bitmap\n");
                    return -1;
                }
                superblock.free_inodes_count--;
                group_descs[group].bg_free_inodes_count--;
                if (ext2_write_superblock() != 0) return -1;
                if (ext2_write_group_descriptors() != 0) return -1;
                kprintf("EXT2: Marked root inode as allocated\n");
            }
        }
        
        /* Allocate a block for the root directory */
        i32 root_block = ext2_alloc_block();
        if (root_block < 0) {
            kprintf("EXT2: Failed to allocate block for root directory\n");
            return -1;
        }

        /* Initialize root inode */
        memset(&root_inode, 0, sizeof(ext2_inode_t));
        root_inode.mode = 0x41ED;  /* Directory with permissions */
        root_inode.size = block_size;
        root_inode.blocks = block_size / 512;
        root_inode.links_count = 2;  /* . and .. */
        root_inode.block[0] = root_block;

        /* Write root inode */
        if (ext2_write_inode(EXT2_ROOT_INODE, &root_inode) != 0) {
            kprintf("EXT2: Failed to write root inode\n");
            return -1;
        }

        /* Create directory entries for . and .. */
        u8 dir_buffer[block_size];
        memset(dir_buffer, 0, block_size);
        
        ext2_dirent_t *dot = (ext2_dirent_t *)dir_buffer;
        dot->inode = EXT2_ROOT_INODE;
        dot->rec_len = 12;
        dot->name_len = 1;
        dot->file_type = 2;  /* Directory */
        dot->name[0] = '.';
        
        ext2_dirent_t *dotdot = (ext2_dirent_t *)((u8 *)dot + 12);
        dotdot->inode = EXT2_ROOT_INODE;  /* Parent is also root */
        dotdot->rec_len = block_size - 12;
        dotdot->name_len = 2;
        dotdot->file_type = 2;  /* Directory */
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        
        /* Write directory block */
        if (write_block(root_block, dir_buffer) < 0) {
            kprintf("EXT2: Failed to write root directory block\n");
            return -1;
        }

        kprintf("EXT2: Root inode initialized successfully\n");
    } else {
        kprintf("EXT2: Root inode already initialized (mode=0x%x)\n", root_inode.mode);
    }

    return 0;
}

i32 ext2_read_inode(u32 inode_num, ext2_inode_t *inode) {
    if (inode_num == 0 || inode_num > superblock.inodes_count) return -1;
    u32 group = (inode_num - 1) / superblock.inodes_per_group;
    u32 index = (inode_num - 1) % superblock.inodes_per_group;
    if (group >= ext2_group_count || !group_descs) return -1;
    u32 inode_table_block = group_descs[group].bg_inode_table + (index / inodes_per_block);
    u8 buffer[block_size];
    if (read_block(inode_table_block, buffer) < 0) return -1;
    memcpy(inode, buffer + (index % inodes_per_block) * EXT2_INODE_SIZE, sizeof(ext2_inode_t));
    return 0;
}

i32 ext2_read_block(u32 block_num, void *buffer) {
    return read_block(block_num, buffer);
}

i32 ext2_write_block(u32 block_num, const void *buffer) {
    return write_block(block_num, buffer);
}

u32 ext2_get_block_size(void) {
    return block_size;
}

i32 ext2_read_data(u32 inode_num, void *buffer, u32 size, u32 offset) {
    if (!buffer || size == 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;
    if (offset >= inode.size) return 0;
    u32 bytes_left = size;
    if (offset + bytes_left > inode.size) bytes_left = inode.size - offset;
    u32 block_offset = offset / block_size;
    u32 block_inner_offset = offset % block_size;
    u32 total_read = 0;
    u8 tmp_block[4096];
    u8 indirect_buffer[4096];
    u32 blocks_per_indirect = block_size / 4;
    
    while (bytes_left > 0) {
        u32 current_block = 0;
        
        /* Direct blocks (0-11) */
        if (block_offset < 12) {
            current_block = inode.block[block_offset];
        }
        /* Single indirect block (block 12) */
        else if (block_offset < 12 + blocks_per_indirect) {
            if (inode.block[12] == 0) break;
            if (read_block(inode.block[12], indirect_buffer) < 0) break;
            u32 indirect_index = block_offset - 12;
            u32 *indirect_ptrs = (u32 *)indirect_buffer;
            current_block = indirect_ptrs[indirect_index];
        }
        else {
            break;  /* Beyond what we support */
        }
        
        if (current_block == 0) break;
        if (read_block(current_block, tmp_block) < 0) break;
        
        u32 chunk = block_size - block_inner_offset;
        if (chunk > bytes_left) chunk = bytes_left;
        memcpy((u8 *)buffer + total_read, tmp_block + block_inner_offset, chunk);
        total_read += chunk;
        bytes_left -= chunk;
        block_offset++;
        block_inner_offset = 0;
    }
    return total_read;
}

u32 ext2_find_inode(const char *path) {
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return 0;
    u32 current_inode = EXT2_ROOT_INODE;
    const char *p = absolute + 1;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *end = p;
        while (*end && *end != '/') end++;
        char name[256];
        u32 len = end - p;
        if (len >= sizeof(name)) return 0;
        memcpy(name, p, len);
        name[len] = 0;
        ext2_inode_t inode;
        if (ext2_read_inode(current_inode, &inode) != 0) return 0;
        u8 buffer[block_size];
        u32 found = 0;
        for (u32 i = 0; i < 12 && inode.block[i]; i++) {
            if (read_block(inode.block[i], buffer) < 0) continue;
            ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
            while ((u8 *)dent < buffer + block_size) {
                /* Validate rec_len to prevent infinite loops */
                if (dent->rec_len == 0 || dent->rec_len > block_size) break;
                if (dent->inode && dent->name_len == len && strncmp(dent->name, name, len) == 0) {
                    current_inode = dent->inode;
                    found = 1;
                    break;
                }
                dent = (ext2_dirent_t *)((u8 *)dent + dent->rec_len);
            }
            if (found) break;
        }
        if (!found) return 0;
        p = end;
    }
    return current_inode;
}

i32 ext2_write_inode(u32 inode_num, ext2_inode_t *inode) {
    if (!inode || inode_num == 0 || inode_num > superblock.inodes_count) return -1;
    u32 group = (inode_num - 1) / superblock.inodes_per_group;
    u32 index = (inode_num - 1) % superblock.inodes_per_group;
    if (group >= ext2_group_count || !group_descs) return -1;
    u32 inode_table_block = group_descs[group].bg_inode_table + (index / inodes_per_block);
    u8 buffer[block_size];
    if (read_block(inode_table_block, buffer) < 0) return -1;
    u32 offset = (index % inodes_per_block) * EXT2_INODE_SIZE;
    memcpy(buffer + offset, inode, sizeof(ext2_inode_t));
    if (write_block(inode_table_block, buffer) < 0) return -1;
    return 0;
}

i32 ext2_alloc_block(void) {
    if (!group_descs) return -1;
    for (u32 group = 0; group < ext2_group_count; group++) {
        u32 bitmap_block = group_descs[group].bg_block_bitmap;
        u8 bitmap[block_size];
        if (read_block(bitmap_block, bitmap) < 0) continue;
        for (u32 i = 0; i < block_size; i++) {
            if (bitmap[i] != 0xFF) {
                for (u32 j = 0; j < 8; j++) {
                    if (!(bitmap[i] & (1 << j))) {
                        u32 block_num = group * blocks_per_group + i * 8 + j;
                        if (block_num >= superblock.blocks_count) continue;
                        bitmap[i] |= (1 << j);
                        if (write_block(bitmap_block, bitmap) < 0) return -1;
                        if (superblock.free_blocks_count > 0) superblock.free_blocks_count--;
                        if (group_descs[group].bg_free_blocks_count > 0) group_descs[group].bg_free_blocks_count--;
                        if (ext2_write_superblock() != 0) return -1;
                        if (ext2_write_group_descriptors() != 0) return -1;
                        return block_num;
                    }
                }
            }
        }
    }
    return -1;
}

i32 ext2_alloc_inode(void) {
    if (!group_descs) return -1;
    for (u32 group = 0; group < ext2_group_count; group++) {
        u32 bitmap_block = group_descs[group].bg_inode_bitmap;
        u8 bitmap[block_size];
        if (read_block(bitmap_block, bitmap) < 0) continue;
        for (u32 i = 0; i < block_size; i++) {
            if (bitmap[i] != 0xFF) {
                for (u32 j = 0; j < 8; j++) {
                    if (!(bitmap[i] & (1 << j))) {
                        u32 inode_num = group * superblock.inodes_per_group + i * 8 + j + 1;
                        if (inode_num > superblock.inodes_count) continue;
                        bitmap[i] |= (1 << j);
                        if (write_block(bitmap_block, bitmap) < 0) return -1;
                        if (superblock.free_inodes_count > 0) superblock.free_inodes_count--;
                        if (group_descs[group].bg_free_inodes_count > 0) group_descs[group].bg_free_inodes_count--;
                        if (ext2_write_superblock() != 0) return -1;
                        if (ext2_write_group_descriptors() != 0) return -1;
                        ext2_inode_t new_inode;
                        memset(&new_inode, 0, sizeof(ext2_inode_t));
                        if (ext2_write_inode(inode_num, &new_inode) != 0) return -1;
                        return inode_num;
                    }
                }
            }
        }
    }
    return -1;
}

i32 ext2_add_directory_entry(u32 dir_inode, const char *name, u32 child_inode) {
    if (!name || dir_inode == 0 || child_inode == 0) return -1;
    ext2_inode_t dir_ino;
    if (ext2_read_inode(dir_inode, &dir_ino) != 0) return -1;
    u32 name_len = strlen(name);
    u8 buffer[block_size];
    for (u32 i = 0; i < 12 && dir_ino.block[i]; i++) {
        if (read_block(dir_ino.block[i], buffer) < 0) continue;
        u8 *pos = buffer;
        while ((u32)(pos - buffer) < block_size) {
            ext2_dirent_t *dent = (ext2_dirent_t *)pos;
            if (dent->rec_len == 0) break;
            u32 used_len = 8 + dent->name_len;
            if (used_len % 4) used_len += 4 - (used_len % 4);
            u32 remaining = dent->rec_len - used_len;
            if (remaining >= 8 + name_len) {
                u8 *new_entry = pos + used_len;
                ext2_dirent_t *new_dent = (ext2_dirent_t *)new_entry;
                new_dent->inode = child_inode;
                new_dent->rec_len = remaining;
                new_dent->name_len = name_len;
                new_dent->file_type = 0;
                memcpy(new_dent->name, name, name_len);
                dent->rec_len = used_len;
                if (write_block(dir_ino.block[i], buffer) < 0) return -1;
                return 0;
            }
            pos += dent->rec_len;
        }
    }
    i32 new_block = ext2_alloc_block();
    if (new_block < 0) return -1;
    memset(buffer, 0, block_size);
    ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
    dent->inode = child_inode;
    dent->rec_len = block_size;
    dent->name_len = name_len;
    dent->file_type = 0;
    memcpy(dent->name, name, name_len);
    if (write_block(new_block, buffer) < 0) return -1;
    for (u32 i = 0; i < 12; i++) {
        if (dir_ino.block[i] == 0) {
            dir_ino.block[i] = new_block;
            break;
        }
    }
    dir_ino.size += block_size;
    dir_ino.blocks += block_size / 512;
    if (ext2_write_inode(dir_inode, &dir_ino) != 0) return -1;
    return 0;
}

i32 ext2_update_inode_size(u32 inode_num, u32 new_size) {
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;
    inode.size = new_size;
    return ext2_write_inode(inode_num, &inode);
}

static i32 ext2_clear_inode(u32 inode_num) {
    if (inode_num == 0 || inode_num > superblock.inodes_count) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;

    /* Free data blocks used by inode */
    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        u32 block_num = inode.block[i];
        u32 group = block_num / blocks_per_group;
        if (group >= ext2_group_count) continue;
        u32 bitmap_block = group_descs[group].bg_block_bitmap;
        u8 bitmap[block_size];
        if (read_block(bitmap_block, bitmap) != 0) continue;
        u32 bit_index = block_num - group * blocks_per_group;
        u32 byte_index = bit_index / 8;
        u32 bit_offset = bit_index % 8;
        bitmap[byte_index] &= ~(1 << bit_offset);
        write_block(bitmap_block, bitmap);
        if (superblock.free_blocks_count < 0xFFFFFFFFu) superblock.free_blocks_count++;
        if (group_descs[group].bg_free_blocks_count < 0xFFFFu) group_descs[group].bg_free_blocks_count++;
    }

    /* Clear inode data */
    memset(&inode, 0, sizeof(ext2_inode_t));
    if (ext2_write_inode(inode_num, &inode) != 0) return -1;

    /* Free inode bitmap */
    u32 group = (inode_num - 1) / superblock.inodes_per_group;
    u32 index = (inode_num - 1) % superblock.inodes_per_group;
    if (group < ext2_group_count) {
        u32 bitmap_block = group_descs[group].bg_inode_bitmap;
        u8 bitmap[block_size];
        if (read_block(bitmap_block, bitmap) != 0) return -1;
        u32 byte_index = index / 8;
        u32 bit_offset = index % 8;
        bitmap[byte_index] &= ~(1 << bit_offset);
        if (write_block(bitmap_block, bitmap) != 0) return -1;
        if (superblock.free_inodes_count < 0xFFFFFFFFu) superblock.free_inodes_count++;
        if (group_descs[group].bg_free_inodes_count < 0xFFFFu) group_descs[group].bg_free_inodes_count++;
    }

    if (ext2_write_superblock() != 0) return -1;
    if (ext2_write_group_descriptors() != 0) return -1;
    return 0;
}

i32 ext2_remove_directory_entry(u32 dir_inode, const char *name) {
    if (!name || dir_inode == 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(dir_inode, &inode) != 0) return -1;
    u8 buffer[block_size];
    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        if (read_block(inode.block[i], buffer) != 0) continue;
        ext2_dirent_t *prev = NULL;
        ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
        while ((u8 *)dent < buffer + block_size) {
            if (dent->rec_len == 0 || dent->rec_len > block_size) break;
            if (dent->inode && dent->name_len == strlen(name) && strncmp(dent->name, name, dent->name_len) == 0) {
                if (!prev) {
                    /* First entry should not be removed for a directory block */
                    return -1;
                }
                prev->rec_len += dent->rec_len;
                if (write_block(inode.block[i], buffer) != 0) return -1;
                return 0;
            }
            prev = dent;
            dent = (ext2_dirent_t *)((u8 *)dent + dent->rec_len);
        }
    }
    return -1;
}

static i32 ext2_parent_and_name(const char *path, char *parent, char *name) {
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return -1;
    const char *last_slash = NULL;
    for (const char *p = absolute; *p; p++) if (*p == '/') last_slash = p;
    if (!last_slash) return -1;
    if (last_slash == absolute) {
        if (strlen(absolute + 1) == 0 || strlen(absolute + 1) >= 256) return -1;
        strcpy(parent, "/");
        strncpy(name, absolute + 1, 255);
        name[255] = 0;
    } else {
        u32 parent_len = last_slash - absolute;
        if (parent_len == 0 || parent_len >= 256) return -1;
        if (strlen(last_slash + 1) == 0 || strlen(last_slash + 1) >= 256) return -1;
        memcpy(parent, absolute, parent_len);
        parent[parent_len] = 0;
        strncpy(name, last_slash + 1, 255);
        name[255] = 0;
    }
    return 0;
}

i32 ext2_unlink(const char *path) {
    if (!path || strcmp(path, ".") == 0) return -1;
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return -1;
    u32 inode_num = ext2_find_inode(absolute);
    if (inode_num == 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;
    if (inode.mode & 0x4000) return -1; /* not a file */
    char parent[256];
    char filename[256];
    if (ext2_parent_and_name(path, parent, filename) != 0) return -1;
    u32 parent_inode_num = ext2_find_inode(parent);
    if (parent_inode_num == 0) return -1;
    if (ext2_remove_directory_entry(parent_inode_num, filename) != 0) return -1;
    return ext2_clear_inode(inode_num);
}

i32 ext2_rmdir(const char *path) {
    if (!path || strcmp(path, ".") == 0) return -1;
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return -1;
    u32 inode_num = ext2_find_inode(absolute);
    if (inode_num == 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) return -1;
    if (!(inode.mode & 0x4000)) return -1; /* not a directory */
    /* Ensure directory is empty except for . and .. */
    u8 buffer[block_size];
    for (u32 i = 0; i < 12 && inode.block[i]; i++) {
        if (read_block(inode.block[i], buffer) != 0) continue;
        ext2_dirent_t *dent = (ext2_dirent_t *)buffer;
        while ((u8 *)dent < buffer + block_size) {
            if (dent->rec_len == 0 || dent->rec_len > block_size) break;
            if (dent->inode && !(dent->name_len == 1 && strncmp(dent->name, ".", 1) == 0) &&
                !(dent->name_len == 2 && strncmp(dent->name, "..", 2) == 0)) {
                return -1;
            }
            dent = (ext2_dirent_t *)((u8 *)dent + dent->rec_len);
        }
    }
    char parent[256];
    char dirname[256];
    if (ext2_parent_and_name(path, parent, dirname) != 0) return -1;
    u32 parent_inode_num = ext2_find_inode(parent);
    if (parent_inode_num == 0) return -1;
    if (ext2_remove_directory_entry(parent_inode_num, dirname) != 0) return -1;
    return ext2_clear_inode(inode_num);
}

i32 ext2_write_data(u32 inode_num, const void *buffer, u32 size) {
    if (!buffer || size == 0) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) != 0) {
        kprintf("[EXT2] write_data: cannot read inode %u\n", inode_num);
        return -1;
    }

    u32 written = 0;
    u32 block_offset = 0;
    u32 blocks_per_indirect = block_size / 4; /* 4-byte pointers */

    const u8 *data = (const u8 *)buffer;
    u8 block_buffer[4096];
    u8 indirect_buffer[4096];

    while (written < size) {
        u32 to_write = size - written;
        if (to_write > block_size) to_write = block_size;

        i32 block_num = -1;

        /* Direct blocks (0-11) */
        if (block_offset < 12) {
            if (inode.block[block_offset] == 0) {
                block_num = ext2_alloc_block();
                if (block_num < 0) {
                    kprintf("[EXT2] write_data: out of blocks (direct)\n");
                    goto write_inode_and_return;
                }
                inode.block[block_offset] = (u32)block_num;
            } else {
                block_num = (i32)inode.block[block_offset];
            }
            block_offset++;
        }
        /* Single indirect block (block 12) */
        else if (block_offset < 12 + blocks_per_indirect) {
            if (inode.block[12] == 0) {
                inode.block[12] = (u32)ext2_alloc_block();
                if (inode.block[12] == (u32)-1) {
                    kprintf("[EXT2] write_data: out of blocks (indirect block allocation)\n");
                    goto write_inode_and_return;
                }
                memset(indirect_buffer, 0, block_size);
            } else {
                if (read_block(inode.block[12], indirect_buffer) < 0) {
                    kprintf("[EXT2] write_data: failed to read indirect block\n");
                    return -1;
                }
            }

            u32 indirect_index = block_offset - 12;
            u32 *indirect_ptrs = (u32 *)indirect_buffer;

            if (indirect_ptrs[indirect_index] == 0) {
                block_num = ext2_alloc_block();
                if (block_num < 0) {
                    kprintf("[EXT2] write_data: out of blocks (indirect data)\n");
                    goto write_inode_and_return;
                }
                indirect_ptrs[indirect_index] = (u32)block_num;

                if (write_block(inode.block[12], indirect_buffer) < 0) {
                    kprintf("[EXT2] write_data: failed to write indirect block\n");
                    return -1;
                }
            } else {
                block_num = (i32)indirect_ptrs[indirect_index];
            }
            block_offset++;
        }
        else {
            kprintf("[EXT2] write_data: file too large (max ~4MB with current impl)\n");
            goto write_inode_and_return;
        }

        if (block_num < 0) {
            kprintf("[EXT2] write_data: invalid block number\n");
            return -1;
        }

        if (to_write == block_size) {
            memcpy(block_buffer, data + written, to_write);
        } else {
            memset(block_buffer, 0, block_size);
            if (read_block((u32)block_num, block_buffer) < 0) {
                kprintf("[EXT2] write_data: failed to read block %u\n", (u32)block_num);
                return -1;
            }
            memcpy(block_buffer, data + written, to_write);
        }

        if (write_block((u32)block_num, block_buffer) < 0) {
            kprintf("[EXT2] write_data: write_block failed for block %u\n", (u32)block_num);
            return -1;
        }

        written += to_write;
    }

write_inode_and_return:
    inode.size = written;
    inode.blocks = (written + 511) / 512;

    if (ext2_write_inode(inode_num, &inode) != 0) {
        kprintf("[EXT2] write_data: write_inode failed\n");
        return written > 0 ? (i32)written : -1;
    }

    return (i32)written;
}

i32 ext2_create_file(const char *path, u32 mode) {
    (void)mode;
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return -1;
    char parent_path[256];
    char filename[256];
    const char *last_slash = absolute;
    for (const char *p = absolute; *p; p++) if (*p == '/') last_slash = p;
    if (last_slash == absolute) {
        if (strlen(absolute + 1) == 0 || strlen(absolute + 1) >= sizeof(filename)) return -1;
        parent_path[0] = '/'; parent_path[1] = 0;
        strncpy(filename, absolute + 1, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = 0;
    } else {
        u32 parent_len = last_slash - absolute;
        if (parent_len == 0 || parent_len >= sizeof(parent_path)) return -1;
        if (strlen(last_slash + 1) == 0 || strlen(last_slash + 1) >= sizeof(filename)) return -1;
        memcpy(parent_path, absolute, parent_len);
        parent_path[parent_len] = 0;
        strncpy(filename, last_slash + 1, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = 0;
    }
    u32 parent_inode_num = ext2_find_inode(parent_path);
    if (parent_inode_num == 0) return -1;
    i32 new_inode_num = ext2_alloc_inode();
    if (new_inode_num < 0) return -1;
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.mode = 0x81A4;
    new_inode.size = 0;
    new_inode.blocks = 0;
    new_inode.links_count = 1;
    if (ext2_write_inode(new_inode_num, &new_inode) != 0) return -1;
    if (ext2_add_directory_entry(parent_inode_num, filename, new_inode_num) != 0) return -1;
    return new_inode_num;
}

i32 ext2_create_directory(const char *path, u32 mode) {
    (void)mode;
    char canonical[256];
    const char *absolute = ext2_canonicalize_path(path, canonical, sizeof(canonical));
    if (!absolute || absolute[0] != '/') return -1;
    char parent_path[256];
    char dirname[256];
    const char *last_slash = absolute;
    for (const char *p = absolute; *p; p++) if (*p == '/') last_slash = p;
    if (last_slash == absolute) {
        if (strlen(absolute + 1) == 0 || strlen(absolute + 1) >= sizeof(dirname)) return -1;
        parent_path[0] = '/'; parent_path[1] = 0;
        strncpy(dirname, absolute + 1, sizeof(dirname) - 1);
        dirname[sizeof(dirname) - 1] = 0;
    } else {
        u32 parent_len = last_slash - absolute;
        if (parent_len == 0 || parent_len >= sizeof(parent_path)) return -1;
        if (strlen(last_slash + 1) == 0 || strlen(last_slash + 1) >= sizeof(dirname)) return -1;
        memcpy(parent_path, absolute, parent_len);
        parent_path[parent_len] = 0;
        strncpy(dirname, last_slash + 1, sizeof(dirname) - 1);
        dirname[sizeof(dirname) - 1] = 0;
    }
    u32 parent_inode_num = ext2_find_inode(parent_path);
    if (parent_inode_num == 0) return -1;
    i32 new_inode_num = ext2_alloc_inode();
    if (new_inode_num < 0) return -1;
    i32 dir_block = ext2_alloc_block();
    if (dir_block < 0) return -1;
    ext2_inode_t new_inode;
    memset(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.mode = 0x41ED;
    new_inode.size = block_size;
    new_inode.blocks = block_size / 512;
    new_inode.links_count = 2;
    new_inode.block[0] = dir_block;
    if (ext2_write_inode(new_inode_num, &new_inode) != 0) return -1;
    u8 dir_buffer[block_size];
    memset(dir_buffer, 0, block_size);
    ext2_dirent_t *dot = (ext2_dirent_t *)dir_buffer;
    dot->inode = new_inode_num;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';
    ext2_dirent_t *dotdot = (ext2_dirent_t *)((u8 *)dot + 12);
    dotdot->inode = parent_inode_num;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    if (write_block(dir_block, dir_buffer) != 0) {
        kprintf("EXT2: create_directory: failed write_block for '%s' (parent='%s', dirname='%s') dir_block=%d\n",
                path, parent_path, dirname, dir_block);
        return -1;
    }

    i32 add_ret = ext2_add_directory_entry(parent_inode_num, dirname, new_inode_num);
    if (add_ret != 0) {
        kprintf("EXT2: create_directory: failed add_directory_entry '%s' in parent_inode=%u (new_inode=%u) ret=%d\n",
                dirname, parent_inode_num, (u32)new_inode_num, add_ret);
        return -1;
    }

    return new_inode_num;
}
