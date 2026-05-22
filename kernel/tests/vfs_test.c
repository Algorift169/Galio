/* vfs_test.c - Kernel VFS tests */
#include "kprintf.h"
#include "vfs.h"
#include "string.h"

void vfs_test(void) {
    kprintf("[KTEST] vfs_test\n");

    const char *dir_path = "/ktest";
    const char *file_path = "/ktest/hello.txt";
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

    if (!vfs_is_dir(dir_path)) {
        kprintf("[KTEST FAIL] vfs_test: vfs_is_dir failed for %s\n", dir_path);
        return;
    }

    kprintf("[KTEST] vfs_test passed\n");
}
