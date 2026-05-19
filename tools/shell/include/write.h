#ifndef SHELL_WRITE_H
#define SHELL_WRITE_H

#include "common.h"

u8 shell_write_command(const char *args, const char *current_dir, u8 privileged);

#endif /* SHELL_WRITE_H */
