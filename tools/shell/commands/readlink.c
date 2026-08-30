#include "readlink.h"
#include "kprintf.h"
#include "string.h"
#include "path.h"
#include "vfs.h"

u8 shell_readlink_command(const char *args, const char *current_dir) {
    if (!args || *args == '\0') {
        kprintf("Usage: readlink <path>\n");
        return 0;
    }

    char path[512];
    if (!path_resolve(current_dir, args, path, sizeof(path))) {
        kprintf("readlink: invalid path\n");
        return 0;
    }

    char target[512];
    if (vfs_readlink(path, target, sizeof(target)) != 0) {
        kprintf("%s\n", target);
        return 1;
    }

    kprintf("readlink: %s not a symlink\n", path);
    return 0;
}
