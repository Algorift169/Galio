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

/* vfs_test.c - Kernel VFS tests */
#include "kprintf.h"
#include "vfs.h"
#include "vfs_core.h"
#include "string.h"

void vfs_test(void) {
    kprintf("[KTEST] vfs_test\n");

    const char *dir_path = "./ktest";
    const char *file_path = "./ktest/hello.txt";
    const char message[] = "kernel VFS test";
    char buffer[32];

    if (!vfs_mkdir(dir_path, 1)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_mkdir failed\n");
        return;
    }

    if (!vfs_create(file_path, 1)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_create failed\n");
        return;
    }

    u32 fd = vfs_open(file_path);
    if (fd == VFS_INVALID_FD) {
        kprintf("[KTEST FAIL] vfs_test: vfs_open failed\n");
        return;
    }

    u32 written = vfs_write(fd, message, sizeof(message));
    vfs_close(fd);
    if (written != sizeof(message)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_write wrote %u bytes\n", written);
        return;
    }

    u32 read = vfs_read(file_path, buffer, sizeof(buffer));
    if (read != sizeof(message)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_read returned %u bytes\n", read);
        return;
    }

    if (strncmp(buffer, message, sizeof(message)) != 0) {
        kprintf("[KTEST FAIL] vfs_test: read contents mismatch\n");
        return;
    }

    if (vfs_core_content_type(file_path) != VFS_CONTENT_TEXT) {
        kprintf("[KTEST FAIL] vfs_test: text content classification failed\n");
        return;
    }

    const char *pdf_path = "./ktest/sample.bin";
    const char pdf_signature[] = "%PDF-1.7";
    if (!vfs_create(pdf_path, 1)) {
        kprintf("[KTEST FAIL] vfs_test: PDF fixture creation failed\n");
        return;
    }
    fd = vfs_open(pdf_path);
    if (fd == VFS_INVALID_FD) {
        kprintf("[KTEST FAIL] vfs_test: PDF fixture open failed\n");
        return;
    }
    written = vfs_write(fd, pdf_signature, sizeof(pdf_signature) - 1);
    vfs_close(fd);
    if (written != sizeof(pdf_signature) - 1 ||
        vfs_core_content_type(pdf_path) != VFS_CONTENT_PDF) {
        kprintf("[KTEST FAIL] vfs_test: PDF signature classification failed\n");
        return;
    }

    if (!vfs_is_dir(dir_path)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_is_dir failed for %s\n", dir_path);
        return;
    }

    memset(buffer, 0xA5, sizeof(buffer));
    if (vfs_read("./dev/zero", buffer, 8) != 8) {
        kprintf("[KTEST FAIL] vfs_test: /dev/zero read failed\n");
        return;
    }
    for (u32 i = 0; i < 8; i++) {
        if (buffer[i] != 0) {
            kprintf("[KTEST FAIL] vfs_test: /dev/zero returned nonzero data\n");
            return;
        }
    }

    struct {
        u32 mode;
        u32 size;
        u32 blocks;
        u32 atime;
        u32 mtime;
        u32 ctime;
    } device_stat;
    if (!vfs_stat("./dev/zero", &device_stat) ||
        (device_stat.mode & VFS_TYPE_MASK) != VFS_TYPE_CHARDEV) {
        kprintf("[KTEST FAIL] vfs_test: /dev/zero stat failed\n");
        return;
    }

    const char *missing_device_path = "./ktest/missing-device";
    if (!vfs_core_create_device_ex(missing_device_path, 0444, 99, 99) ||
        vfs_open(missing_device_path) != VFS_INVALID_FD) {
        kprintf("[KTEST FAIL] vfs_test: unregistered device unexpectedly opened\n");
        return;
    }

    kprintf("[KTEST] vfs_test passed\n");
}
