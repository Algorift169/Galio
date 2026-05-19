#include "new.h"
#include "file.h"
#include "kprintf.h"
#include "string.h"

extern u8 shell_dir_command(const char *args, const char *current_dir, u8 replace, u8 privileged);

u8 shell_new_command(const char *args, const char *current_dir, u8 privileged) {
    if (!args || *args == 0) {
        kprintf("[NEW] Usage: new file <name>[.ext] or new file <path/to/name>[.ext]\n");
        kprintf("[NEW]        new dir <name> or new dir <path/to/name>\n");
        kprintf("[NEW] The 'new' command is a generic creation prefix for future objects.\n");
        return 0;
    }

    if (strncmp(args, "file", 4) == 0) {
        const char *file_args = args + 4;
        if (*file_args == ' ') file_args++;
        return shell_file_command(file_args, current_dir, 1, privileged);
    }

    if (strncmp(args, "dir", 3) == 0) {
        const char *dir_args = args + 3;
        if (*dir_args == ' ') dir_args++;
        return shell_dir_command(dir_args, current_dir, 0, privileged);
    }

    kprintf("[NEW] Unknown target for new: %s\n", args);
    kprintf("[NEW] Supported today: new file <name>[.ext] or new dir <name>\n");
    return 0;
}