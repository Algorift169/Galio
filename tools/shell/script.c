/* SPDX-License-Identifier: AGPL-3.0-only */
#include "script.h"
#include "drift/runtime.h"
#include "drift/environment.h"
#include "path.h"
#include "string.h"
#include "vfs.h"

#define GSH_SCRIPT_MAX_SOURCE 16384

static Environment gsh_environment;
static u8 gsh_environment_initialized;
static gsh_script_command_fn gsh_command_handler;
static void *gsh_command_context;

static void gsh_initialize_environment(gsh_script_command_fn execute, void *context)
{
    if (!gsh_environment_initialized) {
        gsh_environment = environment_create();
        gsh_environment_initialized = 1;
    }
    if (execute != NULL) {
        gsh_command_handler = execute;
        gsh_command_context = context;
        environment_set_command_handler(&gsh_environment, execute, context);
    }
}

int gsh_script_execute_line_with_command(const char *line,
                                         gsh_script_command_fn execute,
                                         void *context)
{
    if (line == NULL || line[0] == '\0') return 0;
    gsh_initialize_environment(execute, context);
    return drift_execute_source(line, &gsh_environment) == 0;
}

int gsh_script_execute_line(const char *line)
{
    return gsh_script_execute_line_with_command(line, NULL, NULL);
}

int gsh_script_run_file(const char *path, const char *current_dir,
                        gsh_script_command_fn execute, void *context)
{
    char resolved[VFS_MAX_PATH];
    char source[GSH_SCRIPT_MAX_SOURCE + 1];
    vfs_entry_t *entry;
    u32 size;

    if (path == NULL || current_dir == NULL || execute == NULL) return -1;
    if (!path_resolve(current_dir, path, resolved, sizeof(resolved))) return -1;
    entry = vfs_find(resolved);
    if (entry == NULL || entry->is_dir) return -1;

    size = entry->size > GSH_SCRIPT_MAX_SOURCE ? GSH_SCRIPT_MAX_SOURCE : entry->size;
    size = vfs_read(resolved, source, size);
    source[size] = '\0';

    gsh_initialize_environment(execute, context);
    return drift_execute_source(source, &gsh_environment);
}
