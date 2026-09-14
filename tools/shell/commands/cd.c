#include "cd.h"
#include "kprintf.h"
#include "path.h"
#include "string.h"
#include "vfs.h"
#include "vfs_core.h"

static const char *skip_spaces(const char *text) {
    while (text && (*text == ' ' || *text == '\t')) text++;
    return text;
}

static u8 has_path_separator(const char *text) {
    while (text && *text) {
        if (*text == '/') return 1;
        text++;
    }
    return 0;
}

static u8 resolve_parent_walk(const char *current_dir, const char *token,
                              char *resolved, u32 resolved_size) {
    char cursor[VFS_MAX_PATH];
    u32 steps = 0;
    u32 i = 0;

    if (!current_dir || !token || !*token || !resolved || resolved_size < 2) {
        return 0;
    }

    while (token[i]) {
        if (token[i] != '.' || token[i + 1] != '.') return 0;
        i += 2;
        steps++;
        if (!token[i]) break;
        if (token[i] != '/') return 0;
        i++;
        if (!token[i]) return 0;
    }

    if (steps == 0) return 0;
    strncpy(cursor, current_dir, sizeof(cursor) - 1);
    cursor[sizeof(cursor) - 1] = 0;

    for (u32 step = 0; step < steps; step++) {
        char parent[VFS_MAX_PATH];
        path_parent(cursor, parent, sizeof(parent));
        strncpy(cursor, parent, sizeof(cursor) - 1);
        cursor[sizeof(cursor) - 1] = 0;
    }

    strncpy(resolved, cursor, resolved_size - 1);
    resolved[resolved_size - 1] = 0;
    return vfs_is_dir(resolved) != 0;
}

typedef struct {
    const char *name;
    char first[VFS_MAX_PATH];
    u32 count;
    u8 print_matches;
} cd_search_t;

static void find_directory(vfs_dentry_t *dentry, cd_search_t *search) {
    if (!dentry || !search || !dentry->inode) return;

    if (strcmp(dentry->name, search->name) == 0 &&
        (dentry->inode->mode & VFS_TYPE_MASK) == VFS_TYPE_DIR) {
        char path[VFS_MAX_PATH];
        vfs_core_build_path(dentry, path);
        if (search->print_matches) {
            kprintf("  %s\n", path);
        }
        search->count++;
        if (search->count == 1) {
            strncpy(search->first, path, sizeof(search->first) - 1);
            search->first[sizeof(search->first) - 1] = 0;
        }
    }

    for (vfs_dentry_t *child = dentry->first_child; child;
         child = child->next_sibling) {
        find_directory(child, search);
    }
}

static u8 resolve_directory(const char *current_dir, const char *token,
                            char *resolved, u32 resolved_size) {
    if (!has_path_separator(token) && strcmp(token, ".") != 0 &&
        strcmp(token, "..") != 0) {
        path_resolve(current_dir, token, resolved, resolved_size);
        if (vfs_is_dir(resolved)) return 1;
    }

    if (resolve_parent_walk(current_dir, token, resolved, resolved_size)) {
        return 1;
    }

    if (has_path_separator(token) || strcmp(token, ".") == 0) {
        path_resolve(current_dir, token, resolved, resolved_size);
        return vfs_is_dir(resolved) != 0;
    }

    vfs_dentry_t *start = vfs_core_lookup(current_dir, 0);
    if (!start) return 0;

    cd_search_t search = {.name = token, .first = {0}, .count = 0};
    find_directory(start, &search);
    if (search.count == 1) {
        strncpy(resolved, search.first, resolved_size - 1);
        resolved[resolved_size - 1] = 0;
        return 1;
    }

    if (search.count > 1) {
        kprintf("[CD] Ambiguous directory '%s'; matches:\n", token);
        search.count = 0;
        search.print_matches = 1;
        find_directory(start, &search);
        return 0;
    }

    return 0;
}

static u8 set_current_directory(const char *resolved, char *current_dir,
                                u32 current_dir_size) {
    if (!resolved || !current_dir || current_dir_size < 2) return 0;
    strncpy(current_dir, resolved, current_dir_size - 1);
    current_dir[current_dir_size - 1] = 0;
    return 1;
}

static u8 reject_unprivileged_root(const char *resolved, u8 privileged,
                                   const char *command) {
    if (privileged || strcmp(resolved, ".") != 0) return 0;
    kprintf("Permission denied: use 'rex %s' to access root\n", command);
    return 1;
}

static u8 parse_single_token(const char *args, char *token, u32 token_size) {
    const char *src = skip_spaces(args);
    u32 length = 0;

    if (!src || !*src || !token || token_size < 2) return 0;
    while (src[length] && src[length] != ' ' && src[length] != '\t' &&
           length + 1 < token_size) {
        token[length] = src[length];
        length++;
    }
    token[length] = 0;
    return *skip_spaces(src + length) == 0;
}

u8 shell_cd_command(const char *args, char *current_dir, u32 current_dir_size,
                    u8 privileged) {
    char token[VFS_MAX_PATH];
    char resolved[VFS_MAX_PATH];
    const char *src = skip_spaces(args);
    u32 length = 0;

    if (!src || *src == 0) {
        strncpy(resolved, privileged ? "." : "./usr/home", sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = 0;
        if (!vfs_is_dir(resolved)) {
            kprintf("[CD] Directory not found: %s\n", resolved);
            return 0;
        }
        if (reject_unprivileged_root(resolved, privileged, "cd")) return 0;
        return set_current_directory(resolved, current_dir, current_dir_size);
    }

    while (src[length] && src[length] != ' ' && src[length] != '\t' &&
           length < sizeof(token) - 1) {
        token[length] = src[length];
        length++;
    }
    token[length] = 0;

    if (*skip_spaces(src + length) != 0) {
        kprintf("Usage: cd <directory>\n");
        return 0;
    }

    if (!resolve_directory(current_dir, token, resolved, sizeof(resolved))) {
        if (!has_path_separator(token) && strcmp(token, ".") != 0 &&
            strcmp(token, "..") != 0) {
            cd_search_t search = {.name = token, .first = {0}, .count = 0};
            vfs_dentry_t *start = vfs_core_lookup(current_dir, 0);
            if (start) find_directory(start, &search);
            if (search.count == 0) {
                kprintf("[CD] Directory not found: %s\n", token);
            }
        } else {
            kprintf("[CD] Directory not found: %s\n", token);
        }
        return 0;
    }

    if (reject_unprivileged_root(resolved, privileged, "cd")) return 0;
    return set_current_directory(resolved, current_dir, current_dir_size);
}

u8 shell_goto_command(const char *args, char *current_dir, u32 current_dir_size,
                      u8 privileged) {
    char token[VFS_MAX_PATH];
    char resolved[VFS_MAX_PATH];

    if (!parse_single_token(args, token, sizeof(token))) {
        kprintf("Usage: goto <directory>\n");
        return 0;
    }

    if (!resolve_directory(current_dir, token, resolved, sizeof(resolved))) {
        kprintf("[GOTO] Directory not found or ambiguous: %s\n", token);
        return 0;
    }

    if (reject_unprivileged_root(resolved, privileged, "goto")) return 0;
    return set_current_directory(resolved, current_dir, current_dir_size);
}
