#include "recycle.h"
#include "kprintf.h"
#include "string.h"
#include "vfs.h"
#include "heap.h"

#define RECYCLE_BIN_DIR "/home/desktop/recycle"
#define RECYCLE_DIR_PARENT "/home/desktop"
#define MAX_RECYCLABLE_SIZE 65536

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

static void normalize_path(char *path) {
    u32 len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
}

static u8 is_root_child_path(const char *path) {
    char parent[VFS_MAX_PATH];
    get_parent_dir(path, parent);
    return strcmp(parent, "/") == 0;
}

static u8 ensure_recycle_bin(void) {
    if (vfs_is_dir(RECYCLE_BIN_DIR)) return 1;
    if (!vfs_is_dir(RECYCLE_DIR_PARENT)) {
        vfs_mkdir(RECYCLE_DIR_PARENT, 1);
    }
    return vfs_mkdir(RECYCLE_BIN_DIR, 1);
}

static void format_recycle_name(char *out_name, const char *base, u32 suffix) {
    if (suffix == 0) {
        strncpy(out_name, base, VFS_MAX_FILENAME - 1);
        out_name[VFS_MAX_FILENAME - 1] = 0;
        return;
    }

    strncpy(out_name, base, VFS_MAX_FILENAME - 1);
    out_name[VFS_MAX_FILENAME - 1] = 0;
    strncat(out_name, "_recycled", VFS_MAX_FILENAME - strlen(out_name) - 1);
    if (suffix > 1) {
        char number[16];
        u32 n = suffix;
        u32 idx = sizeof(number) - 1;
        number[idx] = '\0';
        if (n == 0) {
            idx--;
            number[idx] = '0';
        } else {
            while (n > 0 && idx > 0) {
                idx--;
                number[idx] = '0' + (n % 10);
                n /= 10;
            }
        }
        strncat(out_name, "_", VFS_MAX_FILENAME - strlen(out_name) - 1);
        strncat(out_name, &number[idx], VFS_MAX_FILENAME - strlen(out_name) - 1);
    }
}

static void make_recycle_target(const char *base, char *out_path) {
    strncpy(out_path, RECYCLE_BIN_DIR, VFS_MAX_PATH - 1);
    out_path[VFS_MAX_PATH - 1] = 0;
    strncat(out_path, "/", VFS_MAX_PATH - strlen(out_path) - 1);
    strncat(out_path, base, VFS_MAX_PATH - strlen(out_path) - 1);

    if (!vfs_find(out_path)) return;

    u32 suffix = 1;
    char name[VFS_MAX_FILENAME];
    char candidate[VFS_MAX_PATH];
    while (1) {
        format_recycle_name(name, base, suffix);
        strncpy(candidate, RECYCLE_BIN_DIR, VFS_MAX_PATH - 1);
        candidate[VFS_MAX_PATH - 1] = 0;
        strncat(candidate, "/", VFS_MAX_PATH - strlen(candidate) - 1);
        strncat(candidate, name, VFS_MAX_PATH - strlen(candidate) - 1);
        if (!vfs_find(candidate)) {
            strncpy(out_path, candidate, VFS_MAX_PATH - 1);
            out_path[VFS_MAX_PATH - 1] = 0;
            return;
        }
        suffix++;
    }
}

static u8 recycle_single_path(const char *fullpath, u8 privileged) {
    if (!ensure_recycle_bin()) {
        kprintf("[RECYCLE] Failed to prepare recycle bin\n");
        return 0;
    }

    vfs_entry_t *entry = vfs_find(fullpath);
    if (!entry) {
        kprintf("[RECYCLE] Path not found: %s\n", fullpath);
        return 0;
    }

    if (!privileged && is_root_child_path(fullpath)) {
        kprintf("[RECYCLE] Permission denied: use 'rex recycle %s' to move root-level files\n", fullpath);
        return 0;
    }

    char parent[VFS_MAX_PATH];
    get_parent_dir(fullpath, parent);
    if (strcmp(parent, RECYCLE_BIN_DIR) == 0) {
        kprintf("[RECYCLE] File is already in recycle bin: %s\n", fullpath);
        return 0;
    }

    const char *base = vfs_basename(fullpath);
    if (!base || *base == '\0') {
        kprintf("[RECYCLE] Invalid file name\n");
        return 0;
    }

    char dest[VFS_MAX_PATH];
    make_recycle_target(base, dest);

    if (!vfs_move(fullpath, dest)) {
        kprintf("[RECYCLE] Filesystem move failed: %s -> %s\n", fullpath, dest);
        return 0;
    }

    kprintf("[RECYCLE] Moved %s to %s\n", fullpath, dest);
    return 1;
}

u8 shell_recycle_command(const char *args, const char *current_dir, u8 privileged) {
    if (!args || *skip_spaces(args) == '\0') {
        kprintf("[RECYCLE] Usage: recycle <path1> [path2] ...\n");
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
        normalize_path(fullpath);

        if (fullpath[0] == '\0') {
            kprintf("[RECYCLE] Invalid path: %s\n", arg);
            success = 0;
        } else {
            if (!recycle_single_path(fullpath, privileged)) {
                success = 0;
            }
        }

        // Move to next argument
        arg_start = skip_spaces(arg_end);
    }

    if (success) {
        vfs_fsync();  /* Ensure recycle operations are written to disk */
    }
    
    return success;
}