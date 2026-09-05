/* SPDX-License-Identifier: AGPL-3.0-only */
/* Arithmetic preserves numeric result types where possible and 
returns a neutral value on invalid input. 
Now the operator_apply function handles arithmetic operations on Value 
objects, including addition, subtraction, multiplication, division, and modulo.
It checks the types of the operands and performs the appropriate operation, 
returning a new Value object with the result. If the operands are not 
numeric, or if there is an error (such as division by zero), the 
function returns a null Value and prints an error message to stderr.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "drift/operator.h"


// The require_numeric_pair function checks if both operands are numeric (integer, float, or 
// boolean) and extracts their values as doubles. If either operand is not numeric, it returns 0,
// and the caller can handle the error accordingly. This function is used to ensure that arithmetic
// operations are only performed on valid numeric types, preventing runtime errors and undefined behavior.
static int require_numeric_pair(const Value *left, const Value *right, double *l, double *r)
{
    // Normalize all supported numeric kinds before the arithmetic operation runs.
    if (left == NULL || right == NULL || l == NULL || r == NULL) {
        return 0;
    }

    /*
    We check the types of the left and right operands to determine if they are numeric.
    If they are integers, floats, or booleans, we convert their values to doubles
    and store them in the provided pointers. If either operand is not numeric, we return 0
    to indicate that the operation cannot proceed. This function ensures that arithmetic
    operations are only performed on valid numeric types, preventing runtime errors and undefined
    behavior.
    */
    if (left->type == VALUE_INTEGER) {
        *l = (double)left->integer_value;
    } else if (left->type == VALUE_FLOAT) {
        *l = left->float_value;
    } else if (left->type == VALUE_BOOLEAN) {
        *l = left->boolean_value ? 1.0 : 0.0;
    } else {
        return 0;
    }

    if (right->type == VALUE_INTEGER) {
        *r = (double)right->integer_value;
    } else if (right->type == VALUE_FLOAT) {
        *r = right->float_value;
    } else if (right->type == VALUE_BOOLEAN) {
        *r = right->boolean_value ? 1.0 : 0.0;
    } else {
        return 0;
    }

    return 1;
}

/* Applies numeric arithmetic after validating operand types and reports
 unsupported combinations. */
Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    Value result = value_create_null();

    if (left == NULL || right == NULL) {
        fprintf(stderr, "Runtime Error: Arithmetic operators require numeric values.\n");
        return result;
    }

    if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER) {
        long lval = left->integer_value;
        long rval = right->integer_value;

        if (op == OPERATOR_ADD) {
            result = value_create_integer(lval + rval);
        } else if (op == OPERATOR_SUBTRACT) {
            result = value_create_integer(lval - rval);
        } else if (op == OPERATOR_MULTIPLY) {
            result = value_create_integer(lval * rval);
        } else if (op == OPERATOR_DIVIDE) {
            if (rval == 0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                return result;
            }
            if (lval % rval == 0) {
                result = value_create_integer(lval / rval);
            } else {
                result = value_create_float((double)lval / (double)rval);
            }
        } else if (op == OPERATOR_MODULO) {
            if (rval == 0) {
                fprintf(stderr, "Runtime Error: Modulo by zero.\n");
                return result;
            }
            result = value_create_integer(lval % rval);
        }
        return result;
    }

    double lhs = 0.0; // Initialize lhs and rhs to 0.0 to ensure they have a valid value before use
    double rhs = 0.0; // Initialize lhs and rhs to 0.0 to ensure they have a valid value before use

    // The require_numeric_pair function checks if both operands are numeric and extracts their 
    // values as doubles. If either operand is not numeric, it returns 0, and we print an error message.
    if (op == OPERATOR_ADD || op == OPERATOR_SUBTRACT || op == OPERATOR_MULTIPLY ||
        op == OPERATOR_DIVIDE || op == OPERATOR_MODULO) {
        if (!require_numeric_pair(left, right, &lhs, &rhs)) {
            fprintf(stderr, "Runtime Error: Arithmetic operators require numeric values.\n");
            return result;
        }

        // Perform the arithmetic operation based on the operator type. 
        // If the operation is division or modulo, we check for division by zero and print an
        // error message if necessary. The result is returned as a new Value object.
        if (op == OPERATOR_ADD) {
            result = value_create_float(lhs + rhs);
        } else if (op == OPERATOR_SUBTRACT) {
            result = value_create_float(lhs - rhs);
        } else if (op == OPERATOR_MULTIPLY) {
            result = value_create_float(lhs * rhs);
        } else if (op == OPERATOR_DIVIDE) {
            if (rhs == 0.0) {
                fprintf(stderr, "Runtime Error: Division by zero.\n");
                return result;
            }
            result = value_create_float(lhs / rhs);
        } else if (op == OPERATOR_MODULO) {
            if (rhs == 0.0) {
                fprintf(stderr, "Runtime Error: Modulo by zero.\n");
                return result;
            }
            result = value_create_float(fmod(lhs, rhs));
        }
    }

    return result;
}


/*
Now we define utility functions to check the type of an operator. 
These functions return 1 (true) if the operator matches the specified category,
and 0 (false) otherwise. This allows us to easily determine if an operator is a
comparison, logical, or bitwise operator, which can be useful for parsing and 
evaluating expressions in the Drift programming language.
*/
int operator_is_comparison(OperatorType op)
{
    // Identify equality and ordering operators for parser and dispatcher checks.
    return op == OPERATOR_EQUAL_EQUAL || op == OPERATOR_NOT_EQUAL ||
           op == OPERATOR_GREATER || op == OPERATOR_LESS ||
           op == OPERATOR_GREATER_EQUAL || op == OPERATOR_LESS_EQUAL;
}

int operator_is_logical(OperatorType op)
{
    // Identify operators that combine or negate truth values.
    return op == OPERATOR_AND_AND || op == OPERATOR_OR_OR || op == OPERATOR_NOT;
}

// The operator_is_bitwise function checks if the given operator is a bitwise operator,
// including AND, OR, XOR, NOT, and shift operations. It returns 1 (
// true) if the operator is bitwise, and 0 (false) otherwise. This function is useful
// for determining the type of an operator when parsing and evaluating expressions in the Drift 
// programming language.
int operator_is_bitwise(OperatorType op)
{
    // Identify integer-level operators, including unary NOT and shifts.
    return op == OPERATOR_BITWISE_AND || op == OPERATOR_BITWISE_OR || op == OPERATOR_BITWISE_XOR ||
           op == OPERATOR_BITWISE_NOT || op == OPERATOR_SHIFT_LEFT || op == OPERATOR_SHIFT_RIGHT;
}
