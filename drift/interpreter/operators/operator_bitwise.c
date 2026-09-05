/* SPDX-License-Identifier: AGPL-3.0-only */
/* Bitwise operators are defined for integer operands and reject unsupported types through runtime diagnostics. */

#include <stdio.h>
#include <math.h>

#include "drift/operator.h"
// Checks if a value is an integer or a float that can be safely
// converted to an integer
static int require_integer_value(const Value *val, long *out)
{
    /* Whole-valued floats are accepted because they have an exact integer
       representation; fractional values are rejected. */
    if (val == NULL || out == NULL) {
        return 0;
    }
    if (val->type == VALUE_INTEGER) {
        *out = val->integer_value;
        return 1;
    }
    if (val->type == VALUE_FLOAT) {
        if (floor(val->float_value) == val->float_value) {
            *out = (long)val->float_value;
            return 1;
        }
    }
    return 0;
}

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    /* Handle unary NOT separately, then validate both operands for binary
       operations before applying the selected bitwise transformation. */
    Value result = value_create_null();
    long lval = 0;
    long rval = 0;

    if (op == OPERATOR_BITWISE_NOT) {
        if (left == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise NOT requires an operand.\n");
            return result;
        }
        if (require_integer_value(left, &lval)) {
            result = value_create_integer(~lval);
        } else {
            fprintf(stderr, "Runtime Error: Bitwise NOT requires an integer.\n");
        }
    } else {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise operators require two operands.\n");
            return result;
        }
        if (!require_integer_value(left, &lval) || !require_integer_value(right, &rval)) {
            fprintf(stderr, "Runtime Error: Bitwise operators require integers.\n");
            return result;
        }

        if (op == OPERATOR_BITWISE_AND) {
            result = value_create_integer(lval & rval);
        } else if (op == OPERATOR_BITWISE_OR) {
            result = value_create_integer(lval | rval);
        } else if (op == OPERATOR_BITWISE_XOR) {
            result = value_create_integer(lval ^ rval);
        } else if (op == OPERATOR_SHIFT_LEFT) {
            result = value_create_integer(lval << rval);
        } else if (op == OPERATOR_SHIFT_RIGHT) {
            result = value_create_integer(lval >> rval);
        }
    }

    return result;
}
