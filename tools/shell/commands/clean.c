#include "clean.h"
#include "kprintf.h"
#include "string.h"
#include <string.h>
#include "vfs.h"

#define RECYCLE_BIN_DIR "/home/desktop/recycle"

static const char *skip_spaces(const char *str) {
    while (*str == ' ') str++;
    return str;
}

static void build_fullpath(const char *args, const char *current_dir, char *out_path) {
    const char *src = skip_spaces(args);
    if (*src == '\0') {
        out_path[0] = '\0';
        return;
    }

    if (*src == '/') {
        strncpy(out_path, src, VFS_MAX_PATH - 1);
        out_path[VFS_MAX_PATH - 1] = 0;
        return;
    }

    strncpy(out_path, current_dir, VFS_MAX_PATH - 1);
    out_path[VFS_MAX_PATH - 1] = 0;
    int len = strlen(out_path);
    if (len > 0 && out_path[len - 1] != '/') {
        strncat(out_path, "/", VFS_MAX_PATH - len - 1);
    }
    strncat(out_path, src, VFS_MAX_PATH - strlen(out_path) - 1);
}

u8 shell_clean_command(const char *args, const char *current_dir) {
    if (!args || *skip_spaces(args) == '\0') {
        kprintf("[CLEAN] Usage: clean <dirname>\n");
        return 0;
    }

    char fullpath[VFS_MAX_PATH];
    build_fullpath(args, current_dir, fullpath);

    if (fullpath[0] == '\0') {
        kprintf("[CLEAN] Invalid directory path\n");
        return 0;
    }

    // Allow "rbin" as alias for recycle bin
    char target[VFS_MAX_PATH];
    strncpy(target, skip_spaces(args), VFS_MAX_PATH - 1);
    target[VFS_MAX_PATH - 1] = 0;
    int target_len = strlen(target);
    while (target_len > 0 && target[target_len - 1] == ' ') {
        target[--target_len] = '\0';
    }
    if (strcmp(target, "rbin") == 0) {
        strncpy(fullpath, RECYCLE_BIN_DIR, VFS_MAX_PATH - 1);
        fullpath[VFS_MAX_PATH - 1] = 0;
    }

    if (!vfs_is_dir(fullpath)) {
        kprintf("[CLEAN] Directory not found: %s\n", fullpath);
        return 0;
    }

    if (!vfs_remove_dir_contents(fullpath)) {
        kprintf("[CLEAN] Failed to clean directory: %s\n", fullpath);
        return 0;
    }

    kprintf("[CLEAN] Directory cleared: %s\n", fullpath);
    vfs_fsync();  /* Ensure cleanup is written to disk */
    return 1;
}