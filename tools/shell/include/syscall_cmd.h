#ifndef SHELL_SYSCALL_CMD_H
#define SHELL_SYSCALL_CMD_H

#include "common.h"

u8 shell_syscall_command(const char *args, const char *current_dir, u8 privileged);

#endif
