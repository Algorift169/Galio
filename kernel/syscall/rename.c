#include "syscall.h"
#include "process.h"
#include "string.h"
#include "vfs.h"

i32 syscall_rename(const char *old_path, const char *new_path) {
    process_t *proc = process_current();
    char old_user_path[PROCESS_PATH_MAX];
    char new_user_path[PROCESS_PATH_MAX];
    char old_resolved[PROCESS_PATH_MAX];
    char new_resolved[PROCESS_PATH_MAX];

    if (!proc || !old_path || !new_path ||
        !validate_user_string(old_path, PROCESS_PATH_MAX) ||
        !validate_user_string(new_path, PROCESS_PATH_MAX)) {
        return -14;
    }
    strncpy(old_user_path, old_path, sizeof(old_user_path) - 1);
    old_user_path[sizeof(old_user_path) - 1] = 0;
    strncpy(new_user_path, new_path, sizeof(new_user_path) - 1);
    new_user_path[sizeof(new_user_path) - 1] = 0;
    if (!process_resolve_path(proc->cwd, old_user_path, old_resolved, sizeof(old_resolved)) ||
        !process_resolve_path(proc->cwd, new_user_path, new_resolved, sizeof(new_resolved))) {
        return -22;
    }
    return vfs_move(old_resolved, new_resolved) ? 0 : -2;
}