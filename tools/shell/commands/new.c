/*
 * Galio Kernel
 *
 * Copyright (C) 2026 S.M Israfil
 *
 * This file is part of Galio.
 *
 * Galio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Galio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Galio. If not, see <https://www.gnu.org/licenses/>.
 */

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