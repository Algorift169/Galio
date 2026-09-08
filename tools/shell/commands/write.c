#include "write.h"
#include "editor.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"
#include "auth.h"

static void append_text(char *destination, const char *source, u32 capacity) {
    u32 length;
    if (!destination || !source || capacity == 0) return;
    length = strlen(destination);
    if (length + 1 >= capacity) return;
    strncat(destination, source, capacity - length - 1);
}

static u8 is_root_child_path(const char *path) {
    char normalized[VFS_MAX_PATH];
    char parent[VFS_MAX_PATH];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    if (strcmp(normalized, ".") == 0) return 0;
    path_parent(normalized, parent, sizeof(parent));
    return strcmp(parent, ".") == 0;
}

static u8 is_auth_data_path(const char *path) {
    char normalized[VFS_MAX_PATH];
    if (!path_normalize(path, normalized, sizeof(normalized))) return 0;
    return strcmp(normalized, "./var/account/.udata.galio") == 0 ||
           strcmp(normalized, "var/account/.udata.galio") == 0 ||
           strcmp(normalized, "/var/account/.udata.galio") == 0;
}

static u8 build_filepath(const char *args, const char *current_dir, char *out_path) {
    char input[VFS_MAX_PATH];
    char target_dir[VFS_MAX_PATH];
    char combined[VFS_MAX_PATH];
    char *filename;
    char *separator;

    if (!args || !current_dir || !out_path) return 0;
    strncpy(input, args, sizeof(input) - 1);
    input[sizeof(input) - 1] = 0;
    filename = input;
    while (*filename == ' ' || *filename == '\t') filename++;
    separator = filename;
    while (*separator && *separator != ' ' && *separator != '\t') separator++;
    if (*separator) {
        *separator++ = 0;
        while (*separator == ' ' || *separator == '\t') separator++;
    }
    if (*filename == 0) return 0;

    if (*separator) path_resolve(current_dir, separator, target_dir, sizeof(target_dir));
    else {
        strncpy(target_dir, current_dir, sizeof(target_dir) - 1);
        target_dir[sizeof(target_dir) - 1] = 0;
    }

    strncpy(combined, target_dir, sizeof(combined) - 1);
    combined[sizeof(combined) - 1] = 0;
    if (combined[0] && combined[strlen(combined) - 1] != '/') append_text(combined, "/", sizeof(combined));
    append_text(combined, filename, sizeof(combined));
    return path_resolve(current_dir, combined, out_path, VFS_MAX_PATH) != NULL;
}

u8 shell_write_command(const char *args, const char *current_dir, u8 privileged) {
    char fullpath[VFS_MAX_PATH];
    vfs_entry_t *entry;

    if (!args || !*args || !build_filepath(args, current_dir, fullpath)) {
        kprintf("[WRITE] Usage: write <filename> [path]\n");
        return 0;
    }
    if (!privileged && !auth_is_authorized() &&
        (is_root_child_path(fullpath) || is_auth_data_path(fullpath))) {
        kprintf("[WRITE] Permission denied: use 'rex write %s'\n", fullpath);
        return 0;
    }

    entry = vfs_find(fullpath);
    if (entry && entry->is_dir) {
        kprintf("[WRITE] Error: %s is a directory\n", fullpath);
        return 0;
    }
    if (!entry && !vfs_create(fullpath, 0)) {
        kprintf("[WRITE] Failed to create file: %s\n", fullpath);
        return 0;
    }
    return shell_editor(fullpath);
}
