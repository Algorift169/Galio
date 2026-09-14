#ifndef SHELL_CD_H
#define SHELL_CD_H

#include "common.h"

u8 shell_cd_command(const char *args, char *current_dir, u32 current_dir_size,
					u8 privileged);
u8 shell_goto_command(const char *args, char *current_dir, u32 current_dir_size,
					  u8 privileged);

#endif /* SHELL_CD_H */
