/* SPDX-License-Identifier: AGPL-3.0-only */
/* Increment and decrement return new numeric values, leaving the caller responsible for storing updates. */

#include <stdio.h>

#include "drift/operator.h"

/* Computes increment and decrement results without mutating the source Value. */
Value operator_apply(OperatorType op, const Value *value, const Value *dummy)
{
    /* The third parameter is part of the shared operator signature but is not
       needed for unary increment or decrement. */
    Value result = value_create_null();

    (void)dummy;

    if (value == NULL) {
        fprintf(stderr, "Runtime Error: Increment/decrement operators require an operand.\n");
        return result;
    }

    if (op == OPERATOR_INCREMENT) {
        // Preserve the operand's scalar type while increasing its value.
        if (value->type == VALUE_INTEGER) {
            result = value_create_integer(value->integer_value + 1);
        } else if (value->type == VALUE_FLOAT) {
            result = value_create_float(value->float_value + 1.0);
        } else if (value->type == VALUE_BOOLEAN) {
            result = value_create_boolean(value->boolean_value ? 1 : 0);
        } else {
            fprintf(stderr, "Runtime Error: Increment operator requires a numeric value.\n");
            return result;
        }
    } else if (op == OPERATOR_DECREMENT) {
        // Numeric values decrease by one; booleans use logical negation here.
        if (value->type == VALUE_INTEGER) {
            result = value_create_integer(value->integer_value - 1);
        } else if (value->type == VALUE_FLOAT) {
            result = value_create_float(value->float_value - 1.0);
        } else if (value->type == VALUE_BOOLEAN) {
            result = value_create_boolean(!value->boolean_value);
        } else {
            fprintf(stderr, "Runtime Error: Decrement operator requires a numeric value.\n");
            return result;
        }
    }

    return result;
}
