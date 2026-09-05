/* SPDX-License-Identifier: AGPL-3.0-only */
/* While execution reevaluates one condition before each body pass. */

#include <stdio.h>

#include "drift/array_value.h"
#include "drift/control_flow.h"
#include "drift/interpreter.h"

static int value_is_truthy(const Value *value)
{
    /* Use the same scalar truth rules as conditional execution. */
    if (value == NULL) {
        return 0;
    }
    if (value->type == VALUE_BOOLEAN) {
        return value->boolean_value;
    }
    if (value->type == VALUE_INTEGER) {
        return value->integer_value != 0;
    }
    if (value->type == VALUE_FLOAT) {
        return value->float_value != 0.0;
    }
    if (value->type == VALUE_STRING) {
        return value->string_value != NULL && value->string_value[0] != '\0';
    }
    if (value->type == VALUE_ARRAY) {
        return value->array_value != NULL && value->array_value->length > 0;
    }
    if (value->type == VALUE_INFINITY) {
        return 1;
    }
    return 0;
}

int interpreter_execute_while(WhileStatement *while_statement, Environment *environment)
{
    /*
    Evaluate the condition before every iteration. The body is sent through
    interpreter_execute so assignments update the environment and nested
    statements behave exactly as they do at the top level.
    */
    if (while_statement == NULL || while_statement->condition_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid while statement.\n");
        return 1;
    }

    while (1) {
        int condition_ok = 0;
        Value condition = interpreter_evaluate_expression(environment, while_statement->condition_text, &condition_ok);
        if (!condition_ok) {
            fprintf(stderr, "Runtime Error: Failed to evaluate while condition '%s'.\n", while_statement->condition_text);
            value_free(&condition);
            return 1;
        }

        int should_continue = value_is_truthy(&condition);
        value_free(&condition);
        if (!should_continue) {
            break;
        }

        int continue_loop = 0;
        int break_loop = 0;
        for (size_t i = 0; i < while_statement->body_count; ++i) {
            int result = interpreter_execute(while_statement->body[i], environment);
            if (result == DRIFT_EXECUTION_BREAK) {
                break_loop = 1;
                break;
            }
            if (result == DRIFT_EXECUTION_CONTINUE) {
                continue_loop = 1;
                break;
            }
            if (result != DRIFT_EXECUTION_OK) {
                return result;
            }
        }
        if (break_loop) {
            break;
        }
        if (continue_loop) {
            continue;
        }
    }

    return 0;
}
