/* SPDX-License-Identifier: AGPL-3.0-only */
/* Logical evaluation normalizes booleans and numeric truthiness before
 applying each connective. */

#include <stdio.h>

#include "drift/operator.h"

/* Evaluates logical operators using the language truthiness rules for each operand. */
Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    /* Normalize boolean, integer, and float operands before applying NOT, AND,
       or OR. Unsupported value kinds remain false by initialization. */
    Value result = value_create_boolean(0);

    if (left == NULL) {
        fprintf(stderr, "Runtime Error: Logical operators require a left operand.\n");
        return result;
    }

    // Handle unary NOT operator separately, as it only requires the left 
    // operand. For binary operators, both left and right operands are 
    // required. If the right operand is NULL, an error message is printed.
    if (op == OPERATOR_NOT) {
        if (left->type == VALUE_BOOLEAN) {
            result = value_create_boolean(!left->boolean_value);
        } else {
            result = value_create_boolean(!left->integer_value);
        }
    } else if (right != NULL) {
        int lhs = 0;
        int rhs = 0;

        if (left->type == VALUE_BOOLEAN) {
            lhs = left->boolean_value;
        } else {
            lhs = (left->type == VALUE_INTEGER && left->integer_value != 0) || (left->type == VALUE_FLOAT && left->float_value != 0.0);
        }

        if (right->type == VALUE_BOOLEAN) {
            rhs = right->boolean_value;
        } else {
            rhs = (right->type == VALUE_INTEGER && right->integer_value != 0) || (right->type == VALUE_FLOAT && right->float_value != 0.0);
        }

        if (op == OPERATOR_AND_AND) {
            result = value_create_boolean(lhs && rhs);
        } else if (op == OPERATOR_OR_OR) {
            result = value_create_boolean(lhs || rhs);
        }
    }

    return result;
}
