/* path.c - Shared path helpers for kernel and shell
 * Centralized path normalization, resolution, joining, and basename/parent logic.
 */
#include "path.h"
#include "string.h"
#include <stddef.h>

#define PATH_MAX_COMPONENTS 64
#define PATH_COMPONENT_LEN 256

u8 path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

static char *normalize_components(const char *path, u8 absolute, char *out, u32 out_size) {
    char components[PATH_MAX_COMPONENTS][PATH_COMPONENT_LEN];
    u32 comp_count = 0;
    u32 i = 0;
    u32 length = strlen(path);

    while (i < length) {
        while (i < length && path[i] == '/') {
            i++;
        }
        if (i >= length) break;

        u32 comp_len = 0;
        while (i < length && path[i] != '/' && comp_len + 1 < PATH_COMPONENT_LEN) {
            components[comp_count][comp_len++] = path[i++];
        }
        components[comp_count][comp_len] = 0;

        if (comp_len == 0) {
            continue;
        }

        if (strcmp(components[comp_count], ".") == 0) {
            continue;
        }

        if (strcmp(components[comp_count], "..") == 0) {
            if (comp_count > 0 && strcmp(components[comp_count - 1], "..") != 0) {
                comp_count--;
            } else if (!absolute) {
                if (comp_count < PATH_MAX_COMPONENTS) {
                    comp_count++;
                }
            }
            continue;
        }

        if (comp_count < PATH_MAX_COMPONENTS) {
            comp_count++;
        }
    }

    u32 pos = 0;
    if (absolute) {
        if (out_size == 0) return NULL;
        out[pos++] = '/';
    }

    for (u32 idx = 0; idx < comp_count; idx++) {
        u32 comp_len = strlen(components[idx]);
        if (pos + comp_len + 1 >= out_size) {
            break;
        }
        if (pos > 0 && out[pos - 1] != '/') {
            out[pos++] = '/';
        }
        memcpy(out + pos, components[idx], comp_len);
        pos += comp_len;
    }

    if (pos == 0) {
        if (out_size > 1) {
            out[0] = absolute ? '/' : '.';
            out[1] = 0;
        } else if (out_size == 1) {
            out[0] = 0;
        }
        return out;
    }

    out[pos] = 0;
    return out;
}

char *path_normalize(const char *path, char *out, u32 out_size) {
    if (!out || out_size == 0) return NULL;

    if (!path || path[0] == '\0') {
        if (out_size > 1) {
            out[0] = '/';
            out[1] = 0;
        } else {
            out[0] = 0;
        }
        return out;
    }

    if (path[0] == '/') {
        return normalize_components(path, 1, out, out_size);
    }

    return normalize_components(path, 0, out, out_size);
}

char *path_resolve(const char *cwd, const char *path, char *out, u32 out_size) {
    if (!out || out_size == 0) return NULL;

    if (!path || path[0] == '\0') {
        if (cwd && cwd[0] != '\0') {
            return path_normalize(cwd, out, out_size);
        }
        if (out_size > 1) {
            out[0] = '/';
            out[1] = 0;
        } else {
            out[0] = 0;
        }
        return out;
    }

    if (path_is_absolute(path)) {
        return path_normalize(path, out, out_size);
    }

    char build[PATH_COMPONENT_LEN * 4];
    if (cwd && cwd[0] != '\0') {
        strncpy(build, cwd, sizeof(build) - 1);
        build[sizeof(build) - 1] = 0;
    } else {
        build[0] = '/';
        build[1] = 0;
    }

    u32 build_len = strlen(build);
    if (build_len > 0 && build[build_len - 1] != '/') {
        if (build_len + 1 < sizeof(build)) {
            build[build_len++] = '/';
            build[build_len] = 0;
        }
    }

    u32 add_len = strlen(path);
    if (build_len + add_len < sizeof(build)) {
        memcpy(build + build_len, path, add_len + 1);
    } else {
        if (out_size > 0) out[0] = 0;
        return out;
    }

    return path_normalize(build, out, out_size);
}

char *path_join(const char *base, const char *relative, char *out, u32 out_size) {
    return path_resolve(base, relative, out, out_size);
}

char *path_parent(const char *path, char *out, u32 out_size) {
    if (!path || !out || out_size == 0) return NULL;

    char normalized[PATH_COMPONENT_LEN * 4];
    path_normalize(path, normalized, sizeof(normalized));

    u32 len = strlen(normalized);
    while (len > 1 && normalized[len - 1] == '/') {
        normalized[--len] = 0;
    }

    if (len == 0) {
        if (out_size > 1) {
            out[0] = '/';
            out[1] = 0;
        } else {
            out[0] = 0;
        }
        return out;
    }

    s32 pos = (s32)len - 1;
    while (pos > 0 && normalized[pos] != '/') {
        pos--;
    }

    if (pos <= 0) {
        if (out_size > 1) {
            out[0] = '/';
            out[1] = 0;
        } else {
            out[0] = 0;
        }
        return out;
    }

    u32 copy_len = (u32)pos;
    if (copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, normalized, copy_len);
    out[copy_len] = 0;
    return out;
}

char *path_basename(const char *path, char *out, u32 out_size) {
    if (!path || !out || out_size == 0) return NULL;

    char normalized[PATH_COMPONENT_LEN * 4];
    path_normalize(path, normalized, sizeof(normalized));

    u32 len = strlen(normalized);
    if (len == 0) {
        if (out_size > 1) {
            out[0] = '/';
            out[1] = 0;
        } else {
            out[0] = 0;
        }
        return out;
    }

    s32 pos = (s32)len - 1;
    while (pos > 0 && normalized[pos] != '/') {
        pos--;
    }

    const char *base = (normalized[pos] == '/') ? normalized + pos + 1 : normalized;
    u32 copy_len = strlen(base);
    if (copy_len >= out_size) copy_len = out_size - 1;
    memcpy(out, base, copy_len);
    out[copy_len] = 0;
    return out;
}
