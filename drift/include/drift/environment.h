/* SPDX-License-Identifier: AGPL-3.0-only */
/* The environment is the runtime symbol table shared by expressions and statements. */

#ifndef DRIFT_ENVIRONMENT_H
#define DRIFT_ENVIRONMENT_H

#include "drift/value.h"
#include "drift/ast.h"

#define DRIFT_ENVIRONMENT_CAPACITY 128

typedef struct {
    char *name;
    Value value;
} EnvironmentEntry;

typedef struct {
    EnvironmentEntry entries[DRIFT_ENVIRONMENT_CAPACITY];
    int count;
    FunctionStatement **functions;
    int function_count;
    Value return_value;
    int has_return_value;
} Environment;

/* Initializes an empty fixed-capacity environment with safe value slots. */
Environment environment_create(void);
/* Releases names and values currently stored in the environment. */
void environment_free(Environment *environment);
/* Copies a named value into an existing slot or appends a new binding. */
int environment_set(Environment *environment, const char *name, const Value *value);
/* Looks up a name and copies its value into the caller-provided result. */
int environment_get(const Environment *environment, const char *name, Value *out_value);
/* Reports whether a name is present without copying its associated value. */
int environment_exists(const Environment *environment, const char *name);
/* Registers or replaces a copied function definition in the environment. */
int environment_set_function(Environment *environment, const FunctionStatement *function);
/* Finds a function definition without transferring its environment ownership. */
const FunctionStatement *environment_get_function(const Environment *environment, const char *name);

#endif
