/* SPDX-License-Identifier: AGPL-3.0-only */
/* Each execution iterates over exclusive integer ranges and one-dimensional arrays. */

#include <stdio.h>

#include "drift/array_value.h"
#include "drift/control_flow.h"
#include "drift/each.h"
#include "drift/interpreter.h"


/* Execute the body and propagate control-flow results to the loop runtime. */
static int execute_each_body(EachStatement *each_statement, Environment *environment)
{
    for (size_t i = 0; i < each_statement->body_count; ++i) {
        int result = interpreter_execute(each_statement->body[i], environment);
        if (result != DRIFT_EXECUTION_OK) {
            return result;
        }
    }
    return DRIFT_EXECUTION_OK;
}

/* Store the current item under the loop variable name. */
static int store_each_item(const char *name, const Value *value, Environment *environment)
{
    if (!environment_set(environment, name, value)) {
        return 0;
    }
    return 1;
}

/* Evaluate the source, iterate its values, and execute the loop body. */
int interpreter_execute_each(EachStatement *each_statement, Environment *environment)
{
    int ok = 0;

    if (each_statement == NULL || environment == NULL || each_statement->item_name == NULL || each_statement->source_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid each statement.\n");
        return DRIFT_EXECUTION_ERROR;
    }

    /* Range endpoints are evaluated at runtime and the upper endpoint is excluded. */
    if (each_statement->is_range) {
        Value start_value = interpreter_evaluate_expression(environment, each_statement->start_text, &ok);
        if (!ok || start_value.type != VALUE_INTEGER) {
            value_free(&start_value);
            fprintf(stderr, "Runtime Error: Each range start must be an integer.\n");
            return DRIFT_EXECUTION_ERROR;
        }
        long start = start_value.integer_value;
        value_free(&start_value);

        /* The end value is exclusive, so an empty or reversed range does not iterate. */
        Value end_value = interpreter_evaluate_expression(environment, each_statement->end_text, &ok);
        if (!ok || end_value.type != VALUE_INTEGER) {
            value_free(&end_value);
            fprintf(stderr, "Runtime Error: Each range end must be an integer.\n");
            return DRIFT_EXECUTION_ERROR;
        }
        long end = end_value.integer_value;
        value_free(&end_value);

        /* Store each range value before executing the body. */
        for (long value = start; value < end; ++value) {
            Value item = value_create_integer(value);
            if (!store_each_item(each_statement->item_name, &item, environment)) {
                value_free(&item);
                fprintf(stderr, "Runtime Error: Unable to store each item '%s'.\n", each_statement->item_name);
                return DRIFT_EXECUTION_ERROR;
            }
            value_free(&item);
            int result = execute_each_body(each_statement, environment);
            if (result == DRIFT_EXECUTION_BREAK) {
                return DRIFT_EXECUTION_OK;
            }
            if (result != DRIFT_EXECUTION_OK && result != DRIFT_EXECUTION_CONTINUE) {
                return result;
            }
        }
        return DRIFT_EXECUTION_OK;
    }

    /* Non-range sources must evaluate to one-dimensional arrays. */
    Value source = interpreter_evaluate_expression(environment, each_statement->source_text, &ok);
    if (!ok || source.type != VALUE_ARRAY || source.array_value == NULL || source.array_value->dimension_count != 1) {
        value_free(&source);
        fprintf(stderr, "Runtime Error: Each source must be a one-dimensional array.\n");
        return DRIFT_EXECUTION_ERROR;
    }

    /* Copy each element into the loop variable before executing the body. */
    for (size_t i = 0; i < source.array_value->length; ++i) {
        Value item = value_copy(&source.array_value->elements[i]);
        if (!store_each_item(each_statement->item_name, &item, environment)) {
            value_free(&item);
            value_free(&source);
            fprintf(stderr, "Runtime Error: Unable to store each item '%s'.\n", each_statement->item_name);
            return DRIFT_EXECUTION_ERROR;
        }
        value_free(&item);
        int result = execute_each_body(each_statement, environment);
        if (result == DRIFT_EXECUTION_BREAK) {
            break;
        }
        if (result != DRIFT_EXECUTION_OK && result != DRIFT_EXECUTION_CONTINUE) {
            value_free(&source);
            return result;
        }
    }
    value_free(&source);
    return DRIFT_EXECUTION_OK;
}