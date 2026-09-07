#include "syscall.h"
#include "process.h"
#include "string.h"
#include "vfs.h"

i32 syscall_chmod(const char *path, u32 mode) {
    process_t *proc = process_current();
    char user_path[PROCESS_PATH_MAX];
    char resolved[PROCESS_PATH_MAX];

    if (!proc || !path || !validate_user_string(path, PROCESS_PATH_MAX)) {
        return -14;
    }
    strncpy(user_path, path, sizeof(user_path) - 1);
    user_path[sizeof(user_path) - 1] = 0;
    if (!process_resolve_path(proc->cwd, user_path, resolved, sizeof(resolved))) {
        return -22;
    }
    return vfs_chmod(resolved, mode);
}