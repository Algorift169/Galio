/* SPDX-License-Identifier: AGPL-3.0-only */
/*
Repeat execution evaluates the parsed loop header at runtime and then runs its
body for each counter value. The parser stores bounds and steps as expression
text, so this file is responsible for evaluating those expressions, selecting
the correct bound condition, updating the counter in the environment, and
propagating errors from nested statements.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/interpreter.h"
#include "drift/control_flow.h"
#include "drift/repeat.h"

static void strip_whitespace(char *text)
{
    /* Remove spaces from a copied step expression so shorthand forms such as
       "i + 2" can be compared with the counter syntax consistently. */
    char *read = text;
    char *write = text;

    if (text == NULL) {
        return;
    }

    while (*read != '\0') {
        if (*read != ' ' && *read != '\t' && *read != '\n' && *read != '\r') {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
}

static int parse_counter_step_expression(const char *step_expr, const char *counter_name, long *out_step)
{
    /*
    Recognize step expressions that describe a counter movement directly:
    i++, ++i, i--, --i, i+2, and i-2. Returning zero leaves more general
    expressions to the normal runtime evaluator.
    */
    char *normalized = NULL;
    size_t name_length = 0;
    char *suffix = NULL;
    char *end_ptr = NULL;
    long number = 0;

    if (step_expr == NULL || counter_name == NULL || out_step == NULL) {
        return 0;
    }

    normalized = drift_duplicate_string(step_expr);
    if (normalized == NULL) {
        return 0;
    }
    strip_whitespace(normalized);
    // Compare the normalized expression with the current loop counter name.
    name_length = strlen(counter_name);

    if (strncmp(normalized, counter_name, name_length) == 0) {
        suffix = normalized + name_length;
        if (strcmp(suffix, "++") == 0) {
            *out_step = 1;
            free(normalized);
            return 1;
        }
        if (strcmp(suffix, "--") == 0) {
            *out_step = -1;
            free(normalized);
            return 1;
        }
        if (suffix[0] == '+' || suffix[0] == '-') {
            errno = 0;
            number = strtol(suffix + 1, &end_ptr, 10);
            if (errno == 0 && end_ptr != suffix + 1 && *end_ptr == '\0') {
                *out_step = (suffix[0] == '+') ? number : -number;
                free(normalized);
                return 1;
            }
        }
    }

    if (strncmp(normalized, "++", 2) == 0 && strcmp(normalized + 2, counter_name) == 0) {
        *out_step = 1;
        free(normalized);
        return 1;
    }
    if (strncmp(normalized, "--", 2) == 0 && strcmp(normalized + 2, counter_name) == 0) {
        *out_step = -1;
        free(normalized);
        return 1;
    }

    if (strncmp(normalized, counter_name, name_length) == 0 && normalized[name_length] == '+' && normalized[name_length + 1] == '+') {
        *out_step = 1;
        free(normalized);
        return 1;
    }
    if (strncmp(normalized, counter_name, name_length) == 0 && normalized[name_length] == '-' && normalized[name_length + 1] == '-') {
        *out_step = -1;
        free(normalized);
        return 1;
    }

    free(normalized);
    return 0;
}

/*
Execute one parsed repeat statement. All values created while resolving bounds,
steps, and counters are released in this function; statement bodies are
executed through the main interpreter so nested control flow keeps its normal
semantics.
*/
int interpreter_execute_repeat(RepeatStatement *repeat_statement, Environment *environment)
{
    long start = 0;
    long end = 0;
    long step = 1;
    int loop_ok = 0;
    Value loop_value;

    if (repeat_statement == NULL || repeat_statement->counter_name == NULL) {
        fprintf(stderr, "Runtime Error: Invalid repeat statement.\n");
        return 1;
    }

    if (!repeat_statement->declares_counter && !environment_exists(environment, repeat_statement->counter_name)) {
        fprintf(stderr, "Runtime Error: Loop variable '%s' must be declared with 'var' or before the loop.\n", repeat_statement->counter_name);
        return 1;
    }

    if (!repeat_statement->has_range) {
        // The parser can represent this form, but the runtime currently requires a range header.
        fprintf(stderr, "Runtime Error: Repeat without a range is not supported yet.\n");
        return 1;
    }

    if (repeat_statement->is_infinite) {
        /* Infinite repeat starts at zero and intentionally omits a terminating
           condition. Its optional step still uses the same shorthand and
           expression evaluation rules as finite loops. */
        start = 0;
        if (repeat_statement->has_step) {
            char *step_expr = repeat_statement->step_text;
            if (step_expr != NULL && step_expr[0] != '\0') {
                char *normalized = drift_duplicate_string(step_expr);
                if (normalized != NULL) {
                    strip_whitespace(normalized);
                }
                if (normalized != NULL && parse_counter_step_expression(normalized, repeat_statement->counter_name, &step)) {
                    // The shorthand already produced the final counter increment.
                    /* handled */
                } else if (normalized != NULL) {
                    // General step expressions are evaluated after exposing the initial counter value.
                    Value current = value_create_integer(start);
                    if (!environment_set(environment, repeat_statement->counter_name, &current)) {
                        value_free(&current);
                        free(normalized);
                        fprintf(stderr, "Runtime Error: Unable to initialize loop variable '%s'.\n", repeat_statement->counter_name);
                        return 1;
                    }
                    Value step_eval = interpreter_evaluate_expression(environment, normalized, &loop_ok);
                    value_free(&current);
                    if (!loop_ok || step_eval.type != VALUE_INTEGER) {
                        fprintf(stderr, "Runtime Error: Repeat step value must be an integer.\n");
                        value_free(&step_eval);
                        free(normalized);
                        return 1;
                    }
                    step = step_eval.integer_value - start;
                    value_free(&step_eval);
                    if (step == 0) {
                        fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
                        free(normalized);
                        return 1;
                    }
                }
                free(normalized);
            }
        }

        for (long i = start;; i += step) {
            // Publish the current counter before executing the body for this iteration.
            Value counter_value = value_create_integer(i);
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            int continue_loop = 0;
            int break_loop = 0;
            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                // Stop immediately if any nested statement reports a runtime error.
                int result = interpreter_execute(repeat_statement->body[j], environment);
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

    if (repeat_statement->start_text == NULL || repeat_statement->start_text[0] == '\0') {
        // An omitted start defaults to zero.
        start = 0;
    } else {
        loop_value = interpreter_evaluate_expression(environment, repeat_statement->start_text, &loop_ok);
        if (!loop_ok || loop_value.type != VALUE_INTEGER) {
            fprintf(stderr, "Runtime Error: Repeat start value must be an integer.\n");
            value_free(&loop_value);
            return 1;
        }
        start = loop_value.integer_value;
        value_free(&loop_value);
    }

    if (repeat_statement->end_text == NULL || repeat_statement->end_text[0] == '\0') {
        // An omitted end makes the endpoint equal to the resolved start.
        end = start;
    } else {
        loop_value = interpreter_evaluate_expression(environment, repeat_statement->end_text, &loop_ok);
        if (!loop_ok || loop_value.type != VALUE_INTEGER) {
            fprintf(stderr, "Runtime Error: Repeat end value must be an integer.\n");
            value_free(&loop_value);
            return 1;
        }
        end = loop_value.integer_value;
        value_free(&loop_value);
    }

    if (repeat_statement->has_step) {
        /* An explicit step wins over inferred direction. Counter shorthand is
           handled directly; all other step expressions are evaluated as ints. */
        char *step_expr = repeat_statement->step_text;
        if (step_expr != NULL && step_expr[0] != '\0') {
            char *normalized = drift_duplicate_string(step_expr);
            if (normalized != NULL) {
                strip_whitespace(normalized);
            }

            if (normalized != NULL && parse_counter_step_expression(normalized, repeat_statement->counter_name, &step)) {
                // Use the step extracted from i++, i--, or i +/- number.
                /* handled */
            } else if (normalized != NULL) {
                // Evaluate a general step expression with the counter initialized to start.
                Value current = value_create_integer(start);
                if (!environment_set(environment, repeat_statement->counter_name, &current)) {
                    value_free(&current);
                    free(normalized);
                    fprintf(stderr, "Runtime Error: Unable to initialize loop variable '%s'.\n", repeat_statement->counter_name);
                    return 1;
                }
                Value step_eval = interpreter_evaluate_expression(environment, normalized, &loop_ok);
                value_free(&current);
                if (!loop_ok || step_eval.type != VALUE_INTEGER) {
                    fprintf(stderr, "Runtime Error: Repeat step value must be an integer.\n");
                    value_free(&step_eval);
                    free(normalized);
                    return 1;
                }
                step = step_eval.integer_value - start;
                value_free(&step_eval);
                if (step == 0) {
                    fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
                    free(normalized);
                    return 1;
                }
            }
            free(normalized);
        }
    } else if (repeat_statement->is_exclusive_upper) {
        // Choose ascending or descending default direction for an exclusive upper range.
        step = (start <= end) ? 1 : -1;
    } else if (repeat_statement->is_exclusive_lower) {
        // Choose ascending or descending default direction for an exclusive lower range.
        step = (start >= end) ? -1 : 1;
    } else if (start <= end) {
        step = 1;
    } else {
        step = -1;
    }

    if (step == 0) {
        // A zero step would never make a finite loop progress.
        fprintf(stderr, "Runtime Error: Repeat step cannot be zero.\n");
        return 1;
    }

    if (repeat_statement->is_exclusive_upper) {
        /* Exclude the upper endpoint while still allowing a negative explicit
           step to travel back toward it. */
        for (long i = start; (step > 0) ? (i < end) : (i > end); i += step) {
            Value counter_value = value_create_integer(i);
            // Store a fresh counter value so the body can read the loop variable.
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            int continue_loop = 0;
            int break_loop = 0;
            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                // A nested failure aborts the repeat immediately.
                int result = interpreter_execute(repeat_statement->body[j], environment);
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
    } else if (repeat_statement->is_exclusive_lower) {
        /* The lower-exclusive form stops before the lower endpoint and uses the
           same counter update path as the other finite forms. */
        for (long i = start; i > end; i += step) {
            Value counter_value = value_create_integer(i);
            // Make the current iteration value visible to the body.
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            int continue_loop = 0;
            int break_loop = 0;
            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                // Preserve the first nonzero result from a nested statement.
                int result = interpreter_execute(repeat_statement->body[j], environment);
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
    } else {
        /* Inclusive ranges include the endpoint when the step direction can
           reach it: <= for ascending loops and >= for descending loops. */
        for (long i = start; (step > 0) ? (i <= end) : (i >= end); i += step) {
            Value counter_value = value_create_integer(i);
            // Update the environment before evaluating the body.
            if (!environment_set(environment, repeat_statement->counter_name, &counter_value)) {
                value_free(&counter_value);
                fprintf(stderr, "Runtime Error: Unable to update loop variable '%s'.\n", repeat_statement->counter_name);
                return 1;
            }
            value_free(&counter_value);

            int continue_loop = 0;
            int break_loop = 0;
            for (size_t j = 0; j < repeat_statement->body_count; ++j) {
                int result = interpreter_execute(repeat_statement->body[j], environment);
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
    }

    return 0;
}
