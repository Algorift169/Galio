#ifndef GSH_SCRIPT_H
#define GSH_SCRIPT_H

#include "common.h"

typedef int (*gsh_script_command_fn)(const char *command, void *context);

int gsh_script_run_file(const char *path, const char *current_dir,
                        gsh_script_command_fn execute, void *context);

/* Evaluate one Drift statement while retaining interactive variables. */
int gsh_script_execute_line(const char *line);

#endif