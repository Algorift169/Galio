/* SPDX-License-Identifier: AGPL-3.0-only */
/* Operator APIs accept immutable operands and return owned Value
results for evaluation. */

#ifndef DRIFT_OPERATOR_H
#define DRIFT_OPERATOR_H

#include "drift/value.h"

typedef enum {
    OPERATOR_NONE,
    OPERATOR_ADD,
    OPERATOR_SUBTRACT,
    OPERATOR_MULTIPLY,
    OPERATOR_DIVIDE,
    OPERATOR_MODULO,
    OPERATOR_EQUAL_EQUAL,
    OPERATOR_NOT_EQUAL,
    OPERATOR_GREATER,
    OPERATOR_LESS,
    OPERATOR_GREATER_EQUAL,
    OPERATOR_LESS_EQUAL,
    OPERATOR_AND_AND,
    OPERATOR_OR_OR,
    OPERATOR_NOT,
    OPERATOR_BITWISE_AND,
    OPERATOR_BITWISE_OR,
    OPERATOR_BITWISE_XOR,
    OPERATOR_BITWISE_NOT,
    OPERATOR_SHIFT_LEFT,
    OPERATOR_SHIFT_RIGHT,
    OPERATOR_ASSIGN,
    OPERATOR_ADD_ASSIGN,
    OPERATOR_SUBTRACT_ASSIGN,
    OPERATOR_MULTIPLY_ASSIGN,
    OPERATOR_DIVIDE_ASSIGN,
    OPERATOR_MODULO_ASSIGN,
    OPERATOR_AND_ASSIGN,
    OPERATOR_OR_ASSIGN,
    OPERATOR_XOR_ASSIGN,
    OPERATOR_SHIFT_LEFT_ASSIGN,
    OPERATOR_SHIFT_RIGHT_ASSIGN,
    OPERATOR_INCREMENT,
    OPERATOR_DECREMENT,
    OPERATOR_RANGE,
    OPERATOR_IN,
    OPERATOR_IS
} OperatorType;

Value operator_apply(OperatorType op, const Value *left, const Value *right);
Value operator_apply_identity(const Value *left, const Value *right);
int operator_is_comparison(OperatorType op);
int operator_is_logical(OperatorType op);
int operator_is_bitwise(OperatorType op);

#endif
