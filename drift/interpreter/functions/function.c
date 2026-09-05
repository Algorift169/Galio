/* SPDX-License-Identifier: AGPL-3.0-only */
/* Function execution registers definitions, binds arguments, and carries return values. */

#include <stdio.h>
#include <string.h>

#include "drift/function.h"
#include "drift/interpreter.h"

/* Registers a definition without executing its body at declaration time. */
int interpreter_execute_function(FunctionStatement *statement, Environment *environment)
{
    if (statement == NULL || environment == NULL || statement->name == NULL) {
        fprintf(stderr, "Runtime Error: Invalid function declaration.\n");
        return DRIFT_EXECUTION_ERROR;
    }
    return environment_set_function(environment, statement) ? DRIFT_EXECUTION_OK : DRIFT_EXECUTION_ERROR;
}

/* Evaluates one call argument list, executes the body in a local environment, and returns its value. */
int interpreter_execute_function_call(FunctionCallStatement *statement, Environment *environment)
{
    const FunctionStatement *function;
    Environment local;
    int result = DRIFT_EXECUTION_OK;

    if (statement == NULL || environment == NULL) return DRIFT_EXECUTION_ERROR;
    function = environment_get_function(environment, statement->name);
    if (function == NULL) {
        fprintf(stderr, "Runtime Error: Undefined function '%s'.\n", statement->name);
        return DRIFT_EXECUTION_ERROR;
    }
    if (statement->argument_count != function->parameter_count) {
        fprintf(stderr, "Runtime Error: Function '%s' expects %zu argument(s), got %zu.\n",
                statement->name, function->parameter_count, statement->argument_count);
        return DRIFT_EXECUTION_ERROR;
    }

    local = environment_create();
    for (int i = 0; i < environment->count; ++i) {
        if (!environment_set(&local, environment->entries[i].name, &environment->entries[i].value)) {
            environment_free(&local);
            return DRIFT_EXECUTION_ERROR;
        }
    }
    for (size_t i = 0; i < function->parameter_count; ++i) {
        int ok = 0;
        Value argument = interpreter_evaluate_expression(environment, statement->arguments[i], &ok);
        if (!ok || !environment_set(&local, function->parameters[i], &argument)) {
            value_free(&argument);
            environment_free(&local);
            return DRIFT_EXECUTION_ERROR;
        }
        value_free(&argument);
    }

    for (size_t i = 0; i < function->body_count; ++i) {
        result = interpreter_execute(function->body[i], &local);
        if (result != DRIFT_EXECUTION_OK) break;
    }
    if (result == DRIFT_EXECUTION_RETURN && local.has_return_value) {
        environment->return_value = value_copy(&local.return_value);
        environment->has_return_value = 1;
        result = DRIFT_EXECUTION_OK;
    }
    environment_free(&local);
    return result;
}

/* Evaluates and stores the return expression for the active function call. */
int interpreter_execute_return(ReturnStatement *statement, Environment *environment)
{
    int ok = 0;
    Value value;
    if (statement == NULL || environment == NULL || statement->expression_text == NULL) {
        return DRIFT_EXECUTION_ERROR;
    }
    value = interpreter_evaluate_expression(environment, statement->expression_text, &ok);
    if (!ok) {
        value_free(&value);
        return DRIFT_EXECUTION_ERROR;
    }
    value_free(&environment->return_value);
    environment->return_value = value;
    environment->has_return_value = 1;
    return DRIFT_EXECUTION_RETURN;
}
