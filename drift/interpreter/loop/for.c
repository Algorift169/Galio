/* SPDX-License-Identifier: AGPL-3.0-only */
/* For execution evaluates initialization once, then condition and increment per iteration. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/interpreter.h"
#include "drift/control_flow.h"
#include "drift/operator.h"

static char *trim_copy(const char *text)
{
    /* Copy a clause without surrounding whitespace so names and operators can be compared safely. */
    const char *start = text;
    const char *end;
    size_t length;
    char *result;

    if (text == NULL) {
        return NULL;
    }
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    length = (size_t)(end - start);
    result = (char *)malloc(length + 1U);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

static int execute_for_initialization(const char *text, int declares_counter, Environment *environment)
{
    /* The initialization clause is a normal scalar assignment such as i = 0. */
    char *clause = trim_copy(text);
    char *equals;
    char *name;
    char *expression;
    Value value;
    int ok = 0;

    if (clause == NULL) {
        return 0;
    }
    equals = strchr(clause, '=');
    if (equals == NULL || equals == clause) {
        free(clause);
        fprintf(stderr, "Runtime Error: For initialization must be an assignment.\n");
        return 0;
    }
    *equals = '\0';
    name = trim_copy(clause);
    expression = trim_copy(equals + 1);
    free(clause);
    if (name == NULL || expression == NULL || name[0] == '\0' || expression[0] == '\0') {
        free(name);
        free(expression);
        fprintf(stderr, "Runtime Error: Invalid for initialization.\n");
        return 0;
    }
    if (declares_counter && strncmp(name, "var ", 4) == 0) {
        memmove(name, name + 4, strlen(name + 4) + 1U);
    }
    if (name[0] == '\0' || (!declares_counter && !environment_exists(environment, name))) {
        fprintf(stderr, "Runtime Error: For counter '%s' must be declared with 'var' or before the loop.\n", name);
        free(name);
        free(expression);
        return 0;
    }
    value = interpreter_evaluate_expression(environment, expression, &ok);
    free(expression);
    if (!ok) {
        free(name);
        value_free(&value);
        fprintf(stderr, "Runtime Error: Failed to evaluate for initialization.\n");
        return 0;
    }
    ok = environment_set(environment, name, &value);
    value_free(&value);
    free(name);
    if (!ok) {
        fprintf(stderr, "Runtime Error: Unable to store for initialization.\n");
    }
    return ok;
}

static int execute_for_increment(const char *text, Environment *environment)
{
    /*
    Apply the increment clause after each body pass. Basic loops support i++,
    i--, and compound arithmetic assignments such as i += 1 and i -= 1.
    */
    char *clause = trim_copy(text);
    const char *operator_text = NULL;
    char *operator_position;
    char *name;
    char *expression = NULL;
    Value current = value_create_null();
    Value right = value_create_null();
    Value replacement = value_create_null();
    int ok = 0;
    OperatorType operation = OPERATOR_NONE;

    if (clause == NULL) {
        return 0;
    }
    if (strstr(clause, "++") != NULL) {
        operator_text = "++";
        operation = OPERATOR_INCREMENT;
    } else if (strstr(clause, "--") != NULL) {
        operator_text = "--";
        operation = OPERATOR_DECREMENT;
    } else if (strstr(clause, "+=") != NULL) {
        operator_text = "+=";
        operation = OPERATOR_ADD;
    } else if (strstr(clause, "-=") != NULL) {
        operator_text = "-=";
        operation = OPERATOR_SUBTRACT;
    }
    if (operator_text == NULL) {
        free(clause);
        fprintf(stderr, "Runtime Error: For increment must update its counter.\n");
        return 0;
    }

    operator_position = strstr(clause, operator_text);
    *operator_position = '\0';
    name = trim_copy(clause);
    if (operator_text[1] == '\0') {
        expression = trim_copy(operator_position + 1);
    } else if (operation == OPERATOR_INCREMENT || operation == OPERATOR_DECREMENT) {
        expression = NULL;
    } else {
        expression = trim_copy(operator_position + 2);
    }
    free(clause);
    if (name == NULL || name[0] == '\0' || !environment_get(environment, name, &current)) {
        free(name);
        free(expression);
        value_free(&current);
        fprintf(stderr, "Runtime Error: Undefined for counter.\n");
        return 0;
    }

    if (operation == OPERATOR_INCREMENT || operation == OPERATOR_DECREMENT) {
        replacement = operator_apply(operation, &current, NULL);
    } else {
        right = interpreter_evaluate_expression(environment, expression, &ok);
        if (ok) {
            replacement = operator_apply(operation, &current, &right);
        }
    }
    free(expression);
    value_free(&current);
    value_free(&right);
    if (!ok && operation != OPERATOR_INCREMENT && operation != OPERATOR_DECREMENT) {
        free(name);
        value_free(&replacement);
        fprintf(stderr, "Runtime Error: Failed to evaluate for increment.\n");
        return 0;
    }
    ok = environment_set(environment, name, &replacement);
    value_free(&replacement);
    free(name);
    return ok;
}

int interpreter_execute_for(ForStatement *for_statement, Environment *environment)
{
    /*
    A for loop follows the familiar three-stage order: initialization once,
    condition before every iteration, and increment after every successful body.
    */
    if (for_statement == NULL || environment == NULL || for_statement->condition_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid for statement.\n");
        return 1;
    }
    if (!execute_for_initialization(for_statement->init_text, for_statement->declares_counter, environment)) {
        return 1;
    }

    while (1) {
        int condition_ok = 0;
        Value condition = interpreter_evaluate_expression(environment, for_statement->condition_text, &condition_ok);
        if (!condition_ok) {
            fprintf(stderr, "Runtime Error: Failed to evaluate for condition '%s'.\n", for_statement->condition_text);
            value_free(&condition);
            return 1;
        }
        int should_continue = condition.type == VALUE_BOOLEAN ? condition.boolean_value :
                              condition.type == VALUE_INTEGER ? condition.integer_value != 0 :
                              condition.type == VALUE_FLOAT ? condition.float_value != 0.0 : 0;
        value_free(&condition);
        if (!should_continue) {
            break;
        }

        int break_loop = 0;
        for (size_t i = 0; i < for_statement->body_count; ++i) {
            int result = interpreter_execute(for_statement->body[i], environment);
            if (result == DRIFT_EXECUTION_BREAK) {
                break_loop = 1;
                break;
            }
            if (result == DRIFT_EXECUTION_CONTINUE) {
                break;
            }
            if (result != DRIFT_EXECUTION_OK) {
                return result;
            }
        }
        if (break_loop) {
            break;
        }
        if (!execute_for_increment(for_statement->increment_text, environment)) {
            return 1;
        }
    }
    return 0;
}
