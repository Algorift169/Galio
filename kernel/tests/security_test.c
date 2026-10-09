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

/* security_test.c - Kernel security hardening tests (combined)
 * These tests exercise PID allocation, user pointer validation and basic
 * paging behaviors. They are small and safe to run during boot tests.
 */
#include "kprintf.h"
#include "process.h"
#include "syscall.h"
#include "paging.h"
#include "pmem.h"
#include "security/security.h"
#include "syscall_catalog.h"
#include "string.h"
#include "vfs.h"
#include "vfs_core.h"

void security_test(void) {
    kprintf("[KTEST] Running security tests...\n");

    if (syscall_wait_time(0) != 0 || syscall_wait_time(0x100000000ULL) != -22) {
        kprintf("[KTEST][FAIL] wait-time syscall range check failed\n");
        return;
    }

    if (syscall_catalog_count() != 122u ||
        !syscall_catalog_find("getpid") ||
        syscall_catalog_find("getpid")->status != SYSCALL_STATUS_IMPLEMENTED ||
        syscall_catalog_find("lstat")->status != SYSCALL_STATUS_ENOSYS ||
        syscall_catalog_find("ioctl2")->status != SYSCALL_STATUS_UNDISPATCHED) {
        kprintf("[KTEST][FAIL] Syscall catalog is incomplete or misclassified\n");
        return;
    }
    for (u32 i = 0; i < syscall_catalog_count(); i++) {
        const syscall_catalog_entry_t *entry = syscall_catalog_at(i);
        if (!entry || !entry->name[0] || syscall_catalog_find(entry->name) != entry) {
            kprintf("[KTEST][FAIL] Syscall catalog lookup mismatch at %u\n", i);
            return;
        }
    }

    /* PID allocator uniqueness (allocate few PIDs and ensure no duplicates) */
    u32 seen[8] = {0};
    for (u32 i = 0; i < 8; i++) {
        u32 pid = process_allocate_pid();
        kprintf("[KTEST] Allocated PID %u\n", pid);
        for (u32 j = 0; j < i; j++) {
            if (seen[j] == pid) {
                kprintf("[KTEST][FAIL] PID reused: %u\n", pid);
                return;
            }
        }
        seen[i] = pid;
    }

    /* Validate user pointer rejection for kernel address */
    if (validate_user_ptr((void *)KERNEL_BASE, 0)) {
        kprintf("[KTEST][FAIL] Kernel address accepted as user pointer\n");
        return;
    }

    process_t user_actor;
    process_t other_user;
    process_t root_actor;
    memset(&user_actor, 0, sizeof(user_actor));
    memset(&other_user, 0, sizeof(other_user));
    memset(&root_actor, 0, sizeof(root_actor));
    user_actor.pid = 20;
    user_actor.uid = 1000;
    other_user.pid = 21;
    other_user.uid = 2000;
    root_actor.pid = 22;
    root_actor.uid = UID_ROOT;
    root_actor.capabilities = SECURITY_CAP_ALL;
    u64 audit_start = security_audit_count();
    if (security_may_signal(&user_actor, &other_user)) {
        kprintf("[KTEST][FAIL] Cross-UID signal was allowed\n");
        return;
    }
    process_t idle_target;
    memset(&idle_target, 0, sizeof(idle_target));
    idle_target.pid = 1;
    idle_target.uid = UID_ROOT;
    if (security_may_signal(&root_actor, &idle_target)) {
        kprintf("[KTEST][FAIL] Kernel idle process signal was allowed\n");
        return;
    }
    if (!security_may_signal(&root_actor, &other_user) ||
        security_audit_count() != audit_start + 2) {
        kprintf("[KTEST][FAIL] Capability signal policy/audit failed\n");
        return;
    }

    const char *chmod_test_path = "./security-test-file";
    if (!vfs_create(chmod_test_path, 1)) {
        kprintf("[KTEST FAIL] security_test: unable to create chmod fixture\n");
        return;
    }
    if (security_may_chmod(&user_actor, chmod_test_path) ||
        !security_may_chmod(&root_actor, chmod_test_path)) {
        kprintf("[KTEST][FAIL] chmod owner/capability policy failed\n");
        vfs_unlink(chmod_test_path);
        return;
    }
    if (!security_check_file_access(&user_actor, chmod_test_path, 4u) ||
        security_check_file_access(&user_actor, chmod_test_path, 2u)) {
        kprintf("[KTEST][FAIL] file DAC read/write policy failed\n");
        vfs_unlink(chmod_test_path);
        return;
    }
    if (security_check_file_access(&user_actor, chmod_test_path, 1u) ||
        !vfs_chmod(chmod_test_path, 0755) ||
        !security_check_file_access(&user_actor, chmod_test_path, 1u)) {
        kprintf("[KTEST][FAIL] execute-bit enforcement or chmod failed\n");
        vfs_unlink(chmod_test_path);
        return;
    }
    if (security_check_parent_access(&user_actor, chmod_test_path, 3u) ||
        !security_check_parent_access(&root_actor, chmod_test_path, 3u)) {
        kprintf("[KTEST][FAIL] parent directory write/search policy failed\n");
        vfs_unlink(chmod_test_path);
        return;
    }
    u32 read_only_fd = vfs_core_open_flags(chmod_test_path, VFS_O_RDONLY);
    if (read_only_fd == VFS_INVALID_FD ||
        vfs_core_write(read_only_fd, "x", 1) != (u32)-1) {
        kprintf("[KTEST][FAIL] read-only file descriptor allowed write\n");
        if (read_only_fd != VFS_INVALID_FD) vfs_core_close(read_only_fd);
        vfs_unlink(chmod_test_path);
        return;
    }
    vfs_core_close(read_only_fd);
    u32 write_only_fd = vfs_core_open_flags(chmod_test_path, VFS_O_WRONLY);
    if (write_only_fd == VFS_INVALID_FD ||
        vfs_core_read(write_only_fd, "x", 1) != (u32)-1) {
        kprintf("[KTEST][FAIL] write-only file descriptor allowed read\n");
        if (write_only_fd != VFS_INVALID_FD) vfs_core_close(write_only_fd);
        vfs_unlink(chmod_test_path);
        return;
    }
    vfs_core_close(write_only_fd);
    vfs_unlink(chmod_test_path);

    /* Basic paging checks: create a temporary page directory and map a user page */
    page_directory_t *pd = paging_create_directory();
    if (!pd) {
        kprintf("[KTEST FAIL] security_test: paging_create_directory failed\n");
        return;
    }

    u32 phys = pmem_alloc(1);
    if (!phys) {
        kprintf("[KTEST FAIL] security_test: pmem_alloc failed\n");
        pmem_free((u32)pd->directory, 1);
        return;
    }

    const u32 user_page = 0x40000000u;
    paging_map(pd, user_page, phys, PAGE_PRESENT | PAGE_USER);

    if (!paging_validate_user_range(pd, user_page, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: readable user page rejected\n");
    }
    if (paging_validate_user_range(pd, user_page, 16, 1)) {
        kprintf("[KTEST FAIL] security_test: read-only user page accepted for write\n");
    }
    if (paging_validate_user_range(pd, KERNEL_SPACE_START, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: kernel address accepted as user range\n");
    }
    if (paging_validate_user_range(pd, 0xFFFFF000u, 0x2000u, 0)) {
        kprintf("[KTEST FAIL] security_test: overflowing user range accepted\n");
    }
    if (paging_validate_user_range(pd, user_page + PAGE_SIZE, 16, 0)) {
        kprintf("[KTEST FAIL] security_test: unmapped user page accepted\n");
    }

    paging_unmap(pd, user_page);
    if (pd->tables[(user_page >> 22) & 0x3FFu]) {
        pmem_free((u32)pd->tables[(user_page >> 22) & 0x3FFu], 1);
    }
    pmem_free((u32)pd->directory, 1);
    pmem_free(phys, 1);

    kprintf("[KTEST] Security tests passed (combined checks)\n");
}
