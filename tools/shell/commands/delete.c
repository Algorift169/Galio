#include "delete.h"
#include "kprintf.h"
#include "string.h"
#include "vfs.h"

static const char *skip_spaces(const char *str) {
    while (*str == ' ' || *str == '\t') str++;
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

static void get_parent_dir(const char *path, char *out_parent) {
    const char *last = path;
    const char *scan = path;
    while (*scan) {
        if (*scan == '/') last = scan + 1;
        scan++;
    }

    if (last == path) {
        strncpy(out_parent, "/", VFS_MAX_PATH - 1);
        out_parent[VFS_MAX_PATH - 1] = 0;
        return;
    }

    u32 len = last - path;
    if (len >= VFS_MAX_PATH) len = VFS_MAX_PATH - 1;
    memcpy(out_parent, path, len);
    out_parent[len] = 0;
}

static u8 is_root_child_path(const char *path) {
    char parent[VFS_MAX_PATH];
    get_parent_dir(path, parent);
    return strcmp(parent, "/") == 0;
}

static u8 delete_single_path(const char *fullpath, u8 privileged) {
    if (!vfs_find(fullpath)) {
        kprintf("[DELETE] Path not found: %s\n", fullpath);
        return 0;
    }

    if (!privileged && is_root_child_path(fullpath)) {
        kprintf("[DELETE] Permission denied: use 'rex delete %s' to delete root-level items\n", fullpath);
        return 0;
    }

    if (strcmp(fullpath, "/") == 0) {
        kprintf("[DELETE] Cannot delete root directory\n");
        return 0;
    }

    vfs_entry_t *entry = vfs_find(fullpath);
    if (!entry) {
        kprintf("[DELETE] Path not found: %s\n", fullpath);
        return 0;
    }

    u8 result = 0;
    if (entry->is_dir) {
        result = vfs_remove_recursive(fullpath);
    } else {
        result = vfs_unlink(fullpath);
    }

    if (!result) {
        kprintf("[DELETE] Failed to delete: %s\n", fullpath);
        return 0;
    }

    kprintf("[DELETE] Permanently deleted: %s\n", fullpath);
    return 1;
}

u8 shell_delete_command(const char *args, const char *current_dir, u8 privileged) {
    if (!args || *skip_spaces(args) == '\0') {
        kprintf("[DELETE] Usage: delete <path1> [path2] ...\n");
        return 0;
    }

    const char *arg_start = skip_spaces(args);
    u8 success = 1;

    while (*arg_start) {
        // Find the end of this argument
        const char *arg_end = arg_start;
        while (*arg_end && *arg_end != ' ') arg_end++;
        
        // Extract the argument
        char arg[VFS_MAX_PATH];
        u32 len = arg_end - arg_start;
        if (len >= VFS_MAX_PATH) len = VFS_MAX_PATH - 1;
        memcpy(arg, arg_start, len);
        arg[len] = '\0';

        // Process this path
        char fullpath[VFS_MAX_PATH];
        build_fullpath(arg, current_dir, fullpath);

        if (fullpath[0] == '\0') {
            kprintf("[DELETE] Invalid path: %s\n", arg);
            success = 0;
        } else {
            if (!delete_single_path(fullpath, privileged)) {
                success = 0;
            }
        }

        // Move to next argument
        arg_start = skip_spaces(arg_end);
    }

    if (success) {
        vfs_fsync();  /* Ensure deletions are written to disk */
    }
    
    return success;
}