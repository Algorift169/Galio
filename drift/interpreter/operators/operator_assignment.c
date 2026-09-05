/* SPDX-License-Identifier: AGPL-3.0-only */
/* Assignment writes through the environment; the operator helper itself only evaluates immutable operands. */

#include <stdio.h>

#include "drift/operator.h"

/* Validates assignment operands while leaving environment mutation to the interpreter. */
Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    /* Assignment operators return the value that the interpreter should use;
       this standalone helper never writes to the environment. */
    Value result = value_create_null();

    if (op == OPERATOR_ADD_ASSIGN || op == OPERATOR_SUBTRACT_ASSIGN ||
        op == OPERATOR_MULTIPLY_ASSIGN || op == OPERATOR_DIVIDE_ASSIGN ||
        op == OPERATOR_MODULO_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Assignment operators require operands.\n");
            return result;
        }
        // Compound assignment is handled at parser/interpreter level.
        result = *left;
    } else if (op == OPERATOR_AND_ASSIGN || op == OPERATOR_OR_ASSIGN ||
               op == OPERATOR_XOR_ASSIGN || op == OPERATOR_SHIFT_LEFT_ASSIGN ||
               op == OPERATOR_SHIFT_RIGHT_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise assignment operators require operands.\n");
            return result;
        }
        // Bitwise assignment is handled at parser/interpreter level.
        result = *left;
    } else if (op == OPERATOR_ASSIGN) {
        if (right == NULL) {
            fprintf(stderr, "Runtime Error: Assignment requires a value.\n");
            return result;
        }
        result = *right;
    }

    return result;
}
