/* SPDX-License-Identifier: AGPL-3.0-only */
/* Runtime environments resolve names locally first, then follow the enclosing scope chain. 
The environment is implemented as a fixed-capacity table of name-value pairs,which is sufficient for
the current language design. The environment is not thread-safe, and it is not designed for 
concurrent access. The environment is intended to be used in a single-threaded context, where each 
thread has its own environment. The environment is not designed to be shared between threads,
and it is not designed to be used in a multi-threaded context. The environment is intended to be used
in a single-threaded context, where each thread has its own environment. The environment is not
designed to be shared between threads, and it is not designed to be used in a multi -threaded context. 
The environment is implemented as a fixed-capacity table of name-value pairs, which is sufficient for
the current language design. The environment is not thread-safe, and it is not designed for 
concurrent access. The environment is intended to be used in a single-threaded context, where each
thread has its own environment. The environment is not designed to be shared between threads,
and it is not designed to be used in a multi-threaded context. The environment is intended
to be used in a single-threaded context, where each thread has its own environment.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/environment.h"
#include "drift/parser.h"

/* Centralizes name comparison so all environment operations use identical matching rules. */
static int is_name_equal(const char *left, const char *right)
{
    return strcmp(left, right) == 0;
}

/* Initializes every slot so later cleanup can safely inspect the fixed-capacity table.
TWe need to initialize the environment entries to NULL to avoid dangling pointers and ensure 
that the environment is in a consistent state. This allows for safe cleanup of the environment 
later on, as we can check for NULL entries before attempting to free them. So the function sets the 
count to 0 and initializes each entry's name to NULL and value to a default Value with type VALUE_NULL.
*/
Environment environment_create(void)
{
    Environment environment;
    int i;

    environment.count = 0; // Only the prefix below count contains active bindings.
    environment.functions = NULL;
    environment.function_count = 0;
    environment.return_value = value_create_null();
    environment.has_return_value = 0; // Only the prefix below count contains active bindings.
    environment.command_handler = NULL;
    environment.command_context = NULL;
    for (i = 0; i < DRIFT_ENVIRONMENT_CAPACITY; ++i) {
        environment.entries[i].name = NULL;
        environment.entries[i].value = value_create_string(NULL);
    }

    return environment;
}

void environment_set_command_handler(Environment *environment,
                                      int (*handler)(const char *, void *),
                                      void *context)
{
    if (environment == NULL) return;
    environment->command_handler = handler;
    environment->command_context = context;
}

/* Releases active names and values, leaving the environment empty but still reusable. Using this function
    allows for proper cleanup of the environment without deallocating the Environment structure itself.
*/
void environment_free(Environment *environment)
{
    int i;

    if (environment == NULL) {
        return;
    }

    for (i = 0; i < environment->count; ++i) { // Visit only occupied entries.
        free(environment->entries[i].name);
        value_free(&environment->entries[i].value);
    }

    environment->count = 0;
    free(environment->functions);
    environment->functions = NULL;
    environment->function_count = 0;
    value_free(&environment->return_value);
    environment->has_return_value = 0;
}

/* Replaces an existing binding or appends a copied binding when the name is new. */
int environment_set(Environment *environment, const char *name, const Value *value)
{
    int i;
    char *copy_name;
    Value new_value;

    if (environment == NULL || name == NULL || value == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) { // Prefer replacement to duplicate names.
        if (is_name_equal(environment->entries[i].name, name)) {
            value_free(&environment->entries[i].value);
            new_value = value_copy(value);
            environment->entries[i].value = new_value;
            return 1;
        }
    }

    if (environment->count >= DRIFT_ENVIRONMENT_CAPACITY) {
        return 0;
    }

    copy_name = (char *)malloc(strlen(name) + 1U); // Environment owns its name copy.
    if (copy_name == NULL) {
        return 0;
    }

    strcpy(copy_name, name);
    new_value = value_copy(value);

    environment->entries[environment->count].name = copy_name;
    environment->entries[environment->count].value = new_value;
    environment->count++;
    return 1;
}

/* Finds a binding and returns a copy so reading cannot transfer table ownership. */
int environment_get(const Environment *environment, const char *name, Value *out_value)
{
    int i;

    if (environment == NULL || name == NULL || out_value == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) {
        if (is_name_equal(environment->entries[i].name, name)) {
            *out_value = value_copy(&environment->entries[i].value);
            return 1;
        }
    }

    return 0;
}

/* Checks membership without allocating or copying the stored value. 
This function iterates through the environment's entries and compares the provided name with each 
entry's name using the is_name_equal function. If a match is found, it returns 1 to indicate that
the name exists in the environment. If no match is found after checking all entries, it returns
0 to indicate that the name does not exist in the environment.
*/
int environment_exists(const Environment *environment, const char *name)
{
    int i;

    if (environment == NULL || name == NULL) {
        return 0;
    }

    for (i = 0; i < environment->count; ++i) {
        if (is_name_equal(environment->entries[i].name, name)) {
            return 1;
        }
    }

    return 0;
}

/* Finds a function definition by name without transferring ownership. */
const FunctionStatement *environment_get_function(const Environment *environment, const char *name)
{
    if (environment == NULL || name == NULL) return NULL;
    for (int i = 0; i < environment->function_count; ++i) {
        if (strcmp(environment->functions[i]->name, name) == 0) return environment->functions[i];
    }
    return NULL;
}

/* Stores the parsed function pointer until the source program is released. */
int environment_set_function(Environment *environment, const FunctionStatement *function)
{
    FunctionStatement **items;
    if (environment == NULL || function == NULL || function->name == NULL) return 0;
    for (int i = 0; i < environment->function_count; ++i) {
        if (strcmp(environment->functions[i]->name, function->name) == 0) {
            environment->functions[i] = (FunctionStatement *)function;
            return 1;
        }
    }
    items = (FunctionStatement **)realloc(environment->functions,
        (size_t)(environment->function_count + 1) * sizeof(*items));
    if (items == NULL) return 0;
    environment->functions = items;
    environment->functions[environment->function_count++] = (FunctionStatement *)function;
    return 1;
}
