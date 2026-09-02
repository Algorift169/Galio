#ifndef SHELL_IP_H
#define SHELL_IP_H

#include "common.h"

u8 shell_ip_command(const char *args, const char *current_dir);
u8 shell_ifconfig_command(const char *args, const char *current_dir);

#endif
