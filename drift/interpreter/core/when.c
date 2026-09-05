/* SPDX-License-Identifier: AGPL-3.0-only */
/* When execution compares one subject with ordered case values. */

#include <stdio.h>

#include "drift/control_flow.h"
#include "drift/interpreter.h"
#include "drift/operator.h"
#include "drift/when.h"

static int execute_body(Statement *body, size_t count, Environment *environment)
{
    /* Execute a selected body and propagate runtime or control-flow results. */
    for (size_t i = 0; i < count; ++i) {
        int result = interpreter_execute(body[i], environment);
        if (result != DRIFT_EXECUTION_OK) {
            return result;
        }
    }
    return DRIFT_EXECUTION_OK;
}

// The interpreter_execute_when function evaluates a when statement by first evaluating the subject
// expression and then comparing it against each case value in order. If a match is found, the
// corresponding case body is executed. If no match is found, the optional else body is executed.
int interpreter_execute_when(WhenStatement *statement, Environment *environment)
{
    /* Evaluate the subject once, then execute the first matching case. */
    int ok = 0;
    int matched = 0;
    Value subject;

    if (statement == NULL || environment == NULL || statement->subject_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid when statement.\n");
        return DRIFT_EXECUTION_ERROR;
    }
    subject = interpreter_evaluate_expression(environment, statement->subject_text, &ok);
    if (!ok) {
        value_free(&subject);
        fprintf(stderr, "Runtime Error: Failed to evaluate when subject '%s'.\n", statement->subject_text);
        return DRIFT_EXECUTION_ERROR;
    }

    for (size_t i = 0; i < statement->case_count; ++i) {
        Value case_value = interpreter_evaluate_expression(environment, statement->cases[i].value_text, &ok);
        if (!ok) {
            value_free(&case_value);
            value_free(&subject);
            fprintf(stderr, "Runtime Error: Failed to evaluate when case '%s'.\n", statement->cases[i].value_text);
            return DRIFT_EXECUTION_ERROR;
        }
        /* Reuse the language equality operator for case matching. */
        Value comparison = operator_apply(OPERATOR_EQUAL_EQUAL, &subject, &case_value);
        matched = comparison.type == VALUE_BOOLEAN && comparison.boolean_value;
        value_free(&comparison);
        value_free(&case_value);
        if (matched) {
            int result = execute_body(statement->cases[i].body, statement->cases[i].body_count, environment);
            value_free(&subject);
            return result;
        }
    }
    value_free(&subject);
    return execute_body(statement->else_body, statement->else_count, environment);
}
