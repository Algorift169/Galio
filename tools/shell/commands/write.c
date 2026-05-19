#include "write.h"
#include "editor.h"
#include "kprintf.h"
#include "string.h"
#include "vfs.h"

static void safe_strcat(char *dest, const char *src, u32 max_len) {
    u32 dest_len = strlen(dest);
    u32 copy_len = max_len - dest_len - 1;
    if (copy_len > 0) {
        strncat(dest, src, copy_len);
        dest[max_len - 1] = 0;
    }
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

static void build_filepath(const char *args, const char *current_dir, char *out_path) {
    char filename[256];
    char *path_token = NULL;

    /* Copy arguments and trim */
    strncpy(filename, args, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = 0;
    
    /* Remove leading/trailing spaces */
    char *p = filename;
    while (*p == ' ') p++;
    char *end = p + strlen(p) - 1;
    while (end > p && *end == ' ') end--;
    *(end + 1) = 0;
    
    if (*p == 0) {
        out_path[0] = 0;
        return;
    }
    
    /* Split into filename and optional path token by space */
    char *space = p;
    while (*space && *space != ' ') space++;
    if (*space == ' ') {
        *space = 0;
        path_token = space + 1;
        while (*path_token == ' ') path_token++;
        if (*path_token == 0) path_token = NULL;
    }
    
    /* Determine target directory */
    char target_dir[256];
    if (path_token && path_token[0] == '/') {
        strncpy(target_dir, path_token, sizeof(target_dir) - 1);
    } else if (path_token) {
        strncpy(target_dir, current_dir, sizeof(target_dir) - 1);
        int len = strlen(target_dir);
        if (len > 0 && target_dir[len - 1] != '/')
            safe_strcat(target_dir, "/", sizeof(target_dir));
        safe_strcat(target_dir, path_token, sizeof(target_dir));
    } else {
        strncpy(target_dir, current_dir, sizeof(target_dir) - 1);
    }
    target_dir[sizeof(target_dir) - 1] = 0;
    
    /* Build full path: target_dir + '/' + filename */
    strncpy(out_path, target_dir, 255);
    int len = strlen(out_path);
    if (len > 0 && out_path[len - 1] != '/')
        safe_strcat(out_path, "/", 256);
    safe_strcat(out_path, p, 256);
}

u8 shell_write_command(const char *args, const char *current_dir, u8 privileged) {
    if (!args || *args == 0) {
        kprintf("[WRITE] Usage: write <filename> [path]\n");
        kprintf("[WRITE] Example: write test.txt /home/Documents\n");
        return 0;
    }

    char fullpath[256];
    build_filepath(args, current_dir, fullpath);
    
    if (!privileged && is_root_child_path(fullpath)) {
        kprintf("[WRITE] Permission denied: use 'rex write %s' to write root-level files\n", fullpath);
        return 0;
    }

    /* Ensure the file exists before editing */
    vfs_entry_t *entry = vfs_find(fullpath);
    if (!entry) {
        kprintf("[WRITE] File does not exist. Creating: %s\n", fullpath);
        if (!vfs_create(fullpath, 0)) {
            kprintf("[WRITE] Failed to create file: %s\n", fullpath);
            return 0;
        }
        kprintf("[WRITE] File created successfully.\n");
    } else if (entry->is_dir) {
        kprintf("[WRITE] Error: %s is a directory, not a file\n", fullpath);
        return 0;
    }
    
    return shell_editor(fullpath);
}