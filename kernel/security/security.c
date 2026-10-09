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

/* security.c - Security event handlers used to centralize fatal/soft security
 * responses. These wrappers make it easier to audit and change behavior later.
 */
#include "security/security.h"
#include "kprintf.h"
#include "common.h"
#include "ext2.h"
#include "pit.h"
#include "process/spinlock.h"
#include "string.h"
#include "vfs_core.h"

static security_audit_event_t security_audit_events[SECURITY_AUDIT_CAPACITY];
static spinlock_t security_audit_lock = {0};
static u64 security_audit_next_sequence;

static void security_audit_denial(const process_t *actor, u32 operation,
                                  const char *target, u32 target_id) {
    u64 sequence;
    spin_lock(&security_audit_lock);
    sequence = security_audit_next_sequence++;
    security_audit_event_t *event = &security_audit_events[sequence % SECURITY_AUDIT_CAPACITY];
    event->sequence = sequence;
    event->tick = pit_get_ticks();
    event->pid = actor ? actor->pid : 0;
    event->uid = actor ? actor->uid : 0;
    event->operation = operation;
    event->target_id = target_id;
    event->result = -1;
    if (target) {
        strncpy(event->target, target, sizeof(event->target) - 1);
        event->target[sizeof(event->target) - 1] = 0;
    } else {
        event->target[0] = 0;
    }
    spin_unlock(&security_audit_lock);
    kprintf("[AUDIT] deny pid=%u uid=%u operation=%u target=%s id=%u\n",
            actor ? actor->pid : 0, actor ? actor->uid : 0,
            operation, target ? target : "-", target_id);
}

static u8 security_get_file_metadata(const vfs_dentry_t *dentry, u32 *mode,
                                    u32 *uid, u32 *gid) {
    if (!dentry || !dentry->inode || !mode || !uid || !gid) return 0;
    if (vfs_core_is_disk_mode() &&
        (dentry->inode->mode & VFS_TYPE_MASK) != VFS_TYPE_CHARDEV) {
        ext2_inode_t inode;
        if (ext2_read_inode(dentry->inode->number, &inode) != 0) return 0;
        *mode = inode.mode;
        *uid = inode.uid;
        *gid = inode.gid;
        return 1;
    }
    *mode = dentry->inode->mode;
    *uid = dentry->inode->uid;
    *gid = dentry->inode->gid;
    return 1;
}

static u8 security_mode_allows(const process_t *actor, u32 mode, u32 owner_uid,
                              u32 owner_gid, u32 access) {
    if (security_capable(actor, SECURITY_CAP_DAC_OVERRIDE)) return 1;
    u32 shift = actor->uid == owner_uid ? 6u :
                actor->gid == owner_gid ? 3u : 0u;
    return (((mode >> shift) & 7u) & access) == access;
}

void security_warn(const char *msg) {
    kprintf("SECURITY WARNING: %s\n", msg);
}

void security_panic(const char *msg) {
    kprintf("SECURITY PANIC: %s\n", msg);
    panic(msg);
}

u8 security_capable(const process_t *process, u64 capability) {
    return process && capability &&
           (process->capabilities & capability) == capability;
}

u8 security_may_signal(const process_t *actor, const process_t *target) {
    if (actor && target && target->pid != 1u &&
        (actor->uid == target->uid || security_capable(actor, SECURITY_CAP_KILL))) {
        return 1;
    }
    security_audit_denial(actor, SECURITY_AUDIT_SIGNAL_DENIED, "process",
                          target ? target->pid : 0);
    return 0;
}

u8 security_may_chmod(const process_t *actor, const char *path) {
    u32 mode, owner_uid, owner_gid;
    vfs_dentry_t *dentry = path ? vfs_core_lookup(path, 0) : NULL;
    if (actor && security_get_file_metadata(dentry, &mode, &owner_uid, &owner_gid) &&
        (actor->uid == owner_uid || security_capable(actor, SECURITY_CAP_FOWNER))) {
        return 1;
    }
    security_audit_denial(actor, SECURITY_AUDIT_CHMOD_DENIED, path, 0);
    return 0;
}

u8 security_check_file_access(const process_t *actor, const char *path, u32 access) {
    if (!actor || !path || (access & ~7u)) {
        security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, access);
        return 0;
    }
    vfs_dentry_t *dentry = vfs_core_lookup(path, 0);
    if (!dentry || !dentry->inode) {
        security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, access);
        return 0;
    }
    if (security_capable(actor, SECURITY_CAP_DAC_OVERRIDE)) return 1;

    for (vfs_dentry_t *parent = dentry->parent; parent; parent = parent->parent) {
        if ((parent->inode->mode & VFS_TYPE_MASK) != VFS_TYPE_DIR) continue;
        u32 mode, owner_uid, owner_gid;
        if (!security_get_file_metadata(parent, &mode, &owner_uid, &owner_gid) ||
            !security_mode_allows(actor, mode, owner_uid, owner_gid, 1u)) {
            security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, 1u);
            return 0;
        }
        if (parent == vfs_core_root()) break;
    }

    u32 mode, owner_uid, owner_gid;
    if (!security_get_file_metadata(dentry, &mode, &owner_uid, &owner_gid) ||
        !security_mode_allows(actor, mode, owner_uid, owner_gid, access)) {
        security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, access);
        return 0;
    }
    return 1;
}

u8 security_check_parent_access(const process_t *actor, const char *path, u32 access) {
    if (!actor || !path || (access & ~7u)) {
        security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, access);
        return 0;
    }
    vfs_dentry_t *parent = vfs_core_lookup(path, VFS_LOOKUP_PARENT);
    if (!parent || !parent->inode) {
        security_audit_denial(actor, SECURITY_AUDIT_FILE_DENIED, path, access);
        return 0;
    }
    char parent_path[VFS_MAX_PATH];
    vfs_core_build_path(parent, parent_path);
    return security_check_file_access(actor, parent_path, access);
}

u64 security_audit_count(void) {
    u64 count;
    spin_lock(&security_audit_lock);
    count = security_audit_next_sequence;
    spin_unlock(&security_audit_lock);
    return count;
}

u8 security_audit_read(u64 sequence, security_audit_event_t *event) {
    if (!event) return 0;
    spin_lock(&security_audit_lock);
    u64 next = security_audit_next_sequence;
    u64 oldest = next > SECURITY_AUDIT_CAPACITY ? next - SECURITY_AUDIT_CAPACITY : 0;
    if (sequence < oldest || sequence >= next) {
        spin_unlock(&security_audit_lock);
        return 0;
    }
    *event = security_audit_events[sequence % SECURITY_AUDIT_CAPACITY];
    spin_unlock(&security_audit_lock);
    return event->sequence == sequence;
}
