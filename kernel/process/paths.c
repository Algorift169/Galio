/* paths.c - Current working directory helpers for processes */
#include "process.h"
#include "vfs.h"
#include "string.h"

static u32 normalize_path(const char *input, char *output, u32 size) {
    if (!input || !output || size == 0) {
        return 0;
    }

    char components[64][PROCESS_PATH_MAX];
    u32 comp_count = 0;
    u32 i = 0;
    u32 length = strlen(input);

    while (i < length) {
        while (i < length && input[i] == '/') {
            i++;
        }
        if (i >= length) {
            break;
        }

        u32 comp_len = 0;
        while (i < length && input[i] != '/' && comp_len < PROCESS_PATH_MAX - 1) {
            components[comp_count][comp_len++] = input[i++];
        }
        components[comp_count][comp_len] = 0;

        if (comp_len == 0) {
            continue;
        }

        if (strcmp(components[comp_count], ".") == 0) {
            continue;
        }

        if (strcmp(components[comp_count], "..") == 0) {
            if (comp_count > 0) {
                comp_count--;
            }
            continue;
        }

        if (comp_count < 64) {
            comp_count++;
        }
    }

    if (comp_count == 0) {
        if (size > 1) {
            output[0] = '/';
            output[1] = 0;
            return 1;
        }
        return 0;
    }

    u32 pos = 0;
    output[pos++] = '/';
    for (u32 idx = 0; idx < comp_count; idx++) {
        u32 comp_len = strlen(components[idx]);
        if (pos + comp_len + 1 >= size) {
            break;
        }
        memcpy(output + pos, components[idx], comp_len);
        pos += comp_len;
        if (idx + 1 < comp_count) {
            output[pos++] = '/';
        }
    }
    output[pos] = 0;
    return 1;
}

char *process_resolve_path(const char *cwd, const char *path, char *output, u32 output_size) {
    if (!output || output_size == 0) {
        return NULL;
    }

    if (!path || path[0] == '\0') {
        if (cwd && cwd[0]) {
            strncpy(output, cwd, output_size - 1);
            output[output_size - 1] = 0;
            return output;
        }
        if (output_size > 1) {
            output[0] = '/';
            output[1] = 0;
            return output;
        }
        return NULL;
    }

    char build[PROCESS_PATH_MAX];
    if (path[0] == '/') {
        strncpy(build, path, PROCESS_PATH_MAX - 1);
        build[PROCESS_PATH_MAX - 1] = 0;
    } else {
        if (!cwd || cwd[0] != '/') {
            build[0] = '/';
            build[1] = 0;
        } else {
            strncpy(build, cwd, PROCESS_PATH_MAX - 1);
            build[PROCESS_PATH_MAX - 1] = 0;
        }

        u32 build_len = strlen(build);
        if (build_len > 0 && build[build_len - 1] != '/') {
            if (build_len + 1 < PROCESS_PATH_MAX) {
                build[build_len++] = '/';
                build[build_len] = 0;
            }
        }

        u32 add_len = strlen(path);
        if (build_len + add_len < PROCESS_PATH_MAX) {
            memcpy(build + build_len, path, add_len + 1);
        } else {
            return NULL;
        }
    }

    normalize_path(build, output, output_size);
    return output;
}

u32 process_chdir(const char *path) {
    process_t *proc = process_current();
    if (!proc || !path) {
        return (u32)-1;
    }

    char resolved[PROCESS_PATH_MAX];
    if (!process_resolve_path(proc->cwd, path, resolved, PROCESS_PATH_MAX)) {
        return (u32)-1;
    }

    if (!vfs_is_dir(resolved)) {
        return (u32)-1;
    }

    strncpy(proc->cwd, resolved, PROCESS_PATH_MAX - 1);
    proc->cwd[PROCESS_PATH_MAX - 1] = 0;
    return 0;
}

u32 process_getcwd(char *buffer, u32 size) {
    process_t *proc = process_current();
    if (!proc || !buffer || size == 0) {
        return 0;
    }
    u32 len = strlen(proc->cwd);
    if (len + 1 > size) {
        len = size - 1;
    }
    memcpy(buffer, proc->cwd, len);
    buffer[len] = 0;
    return len;
}
