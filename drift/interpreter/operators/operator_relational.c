/* SPDX-License-Identifier: AGPL-3.0-only */
/* Relational operators compare like-typed scalars and keep unsupported comparisons false with diagnostics. */

#include <stdio.h>
#include <string.h>

#include "drift/operator.h"

/* Compares compatible scalar operands and returns a boolean result. */
Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    /* Compare integers, strings, and booleans using their native operations.
       Mixed or unsupported types only support equality and inequality. */
    Value result = value_create_boolean(0);

    if (left == NULL || right == NULL) {
        fprintf(stderr, "Runtime Error: Comparison operators require left and right operands.\n");
        return result;
    }

    if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER) {
        // Integer ordering is performed without converting through floating point.
        long lval = left->integer_value;
        long rval = right->integer_value;
        if (op == OPERATOR_EQUAL_EQUAL) {
            result = value_create_boolean(lval == rval);
        } else if (op == OPERATOR_NOT_EQUAL) {
            result = value_create_boolean(lval != rval);
        } else if (op == OPERATOR_GREATER) {
            result = value_create_boolean(lval > rval);
        } else if (op == OPERATOR_LESS) {
            result = value_create_boolean(lval < rval);
        } else if (op == OPERATOR_GREATER_EQUAL) {
            result = value_create_boolean(lval >= rval);
        } else if (op == OPERATOR_LESS_EQUAL) {
            result = value_create_boolean(lval <= rval);
        }
        return result;
    }

    if (left->type == VALUE_STRING && right->type == VALUE_STRING) {
        // Strings use lexicographic ordering from strcmp.
        const char *ls = left->string_value ? left->string_value : "";
        const char *rs = right->string_value ? right->string_value : "";
        int cmp = strcmp(ls, rs);
        if (op == OPERATOR_EQUAL_EQUAL) {
            result = value_create_boolean(cmp == 0);
        } else if (op == OPERATOR_NOT_EQUAL) {
            result = value_create_boolean(cmp != 0);
        } else if (op == OPERATOR_GREATER) {
            result = value_create_boolean(cmp > 0);
        } else if (op == OPERATOR_LESS) {
            result = value_create_boolean(cmp < 0);
        } else if (op == OPERATOR_GREATER_EQUAL) {
            result = value_create_boolean(cmp >= 0);
        } else if (op == OPERATOR_LESS_EQUAL) {
            result = value_create_boolean(cmp <= 0);
        }
        return result;
    }

    if (left->type == VALUE_BOOLEAN && right->type == VALUE_BOOLEAN) {
        // Boolean operands support equality checks only.
        if (op == OPERATOR_EQUAL_EQUAL) {
            result = value_create_boolean(left->boolean_value == right->boolean_value);
        } else if (op == OPERATOR_NOT_EQUAL) {
            result = value_create_boolean(left->boolean_value != right->boolean_value);
        }
        return result;
    }

    if (op == OPERATOR_EQUAL_EQUAL) {
        result = value_create_boolean(left->type == right->type);
    } else if (op == OPERATOR_NOT_EQUAL) {
        result = value_create_boolean(left->type != right->type);
    }

    return result;
}
