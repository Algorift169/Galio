#include "tree.h"
#include "kprintf.h"
#include "vfs.h"

u8 shell_tree_command(const char *current_dir) {
    if (!current_dir || *current_dir == 0) {
        current_dir = "/";
    }
    vfs_tree_dir(current_dir);
    return 1;
}
