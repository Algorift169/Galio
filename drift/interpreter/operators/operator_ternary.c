/* SPDX-License-Identifier: AGPL-3.0-only */
/* Ternary evaluation computes truthiness once, then returns a copy of the selected branch. */

#include <stddef.h>

#include "drift/operator_ternary.h"

/* Converts supported value types to truthiness, then copies the selected branch. */
Value operator_apply_ternary(const Value *condition, const Value *true_value, const Value *false_value)
{
    /* Evaluate the condition once, then copy only the selected branch so the
       returned Value owns its data independently of both operands. */
    int is_truthy = 0;

    if (condition != NULL) { // A missing condition remains false by initialization.
        if (condition->type == VALUE_BOOLEAN) {
            is_truthy = condition->boolean_value;
        } else if (condition->type == VALUE_INTEGER) {
            is_truthy = condition->integer_value != 0;
        } else if (condition->type == VALUE_FLOAT) {
            is_truthy = condition->float_value != 0.0;
        } else if (condition->type == VALUE_STRING) {
            is_truthy = condition->string_value != NULL && condition->string_value[0] != '\0';
        } else if (condition->type == VALUE_ARRAY) {
            is_truthy = condition->array_value != NULL;
        } else if (condition->type == VALUE_INFINITY) {
            is_truthy = 1;
        }
    }

    if (is_truthy) { // Copying prevents the result from borrowing branch ownership.
        return value_copy(true_value);
    }

    return value_copy(false_value);
}