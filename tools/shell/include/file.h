#ifndef SHELL_FILE_H
#define SHELL_FILE_H

#include "common.h"

u8 shell_file_command(const char *args, const char *current_dir, u8 replace, u8 privileged);

#endif /* SHELL_FILE_H */
