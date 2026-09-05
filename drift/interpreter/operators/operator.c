/* SPDX-License-Identifier: AGPL-3.0-only */
/* Dispatches operator evaluation to category-specific implementations without owning operands. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "drift/operator.h"
#include "drift/array_value.h"

/*
Each operator category has its own implementation so validation and result
construction stay local. The dispatcher below only selects the correct helper.
*/
static Value operator_apply_arithmetic(OperatorType op, const Value *left, const Value *right);
static Value operator_apply_relational(OperatorType op, const Value *left, const Value *right);
static Value operator_apply_logical(OperatorType op, const Value *left, const Value *right);
static Value operator_apply_bitwise(OperatorType op, const Value *left, const Value *right);
static Value operator_apply_assignment(OperatorType op, const Value *left, const Value *right);
static Value operator_apply_increment(OperatorType op, const Value *value);
static Value operator_apply_range(OperatorType op, const Value *start, const Value *end);
static Value operator_apply_membership(OperatorType op, const Value *container, const Value *member);

int operator_is_comparison(OperatorType op)
{
    // Comparison operators produce boolean results from equality or ordering tests.
    return op == OPERATOR_EQUAL_EQUAL || op == OPERATOR_NOT_EQUAL ||
           op == OPERATOR_GREATER || op == OPERATOR_LESS ||
           op == OPERATOR_GREATER_EQUAL || op == OPERATOR_LESS_EQUAL;
}

int operator_is_logical(OperatorType op)
{
    // Logical operators consume truthy values and return a boolean result.
    return op == OPERATOR_AND_AND || op == OPERATOR_OR_OR || op == OPERATOR_NOT;
}

int operator_is_bitwise(OperatorType op)
{
    // Bitwise operators work on integer representations, including shifts and NOT.
    return op == OPERATOR_BITWISE_AND || op == OPERATOR_BITWISE_OR ||
           op == OPERATOR_BITWISE_XOR || op == OPERATOR_BITWISE_NOT ||
           op == OPERATOR_SHIFT_LEFT || op == OPERATOR_SHIFT_RIGHT;
}

Value operator_apply(OperatorType op, const Value *left, const Value *right)
{
    /* Route the enum ranges to the matching implementation. No operand is
       stored or freed here; ownership remains with the caller. */
    if (op >= OPERATOR_ADD && op <= OPERATOR_MODULO) {
        return operator_apply_arithmetic(op, left, right);
    } else if (op >= OPERATOR_EQUAL_EQUAL && op <= OPERATOR_LESS_EQUAL) {
        return operator_apply_relational(op, left, right);
    } else if (op >= OPERATOR_AND_AND && op <= OPERATOR_NOT) {
        return operator_apply_logical(op, left, right);
    } else if (op >= OPERATOR_BITWISE_AND && op <= OPERATOR_SHIFT_RIGHT) {
        return operator_apply_bitwise(op, left, right);
    } else if (op >= OPERATOR_ASSIGN && op <= OPERATOR_SHIFT_RIGHT_ASSIGN) {
        return operator_apply_assignment(op, left, right);
    } else if (op == OPERATOR_INCREMENT || op == OPERATOR_DECREMENT) {
        return operator_apply_increment(op, left);
    } else if (op == OPERATOR_RANGE) {
        return operator_apply_range(op, left, right);
    } else if (op == OPERATOR_IN) {
        return operator_apply_membership(op, left, right);
    } else if (op == OPERATOR_IS) {
        return operator_apply_identity(left, right);
    }

    return value_create_null();
}

static int require_integer_value(const Value *val, long *out)
{
    /* Bitwise operations accept integers and whole-valued floats, converting
       the latter only after confirming they have no fractional component. */
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

static int require_numeric_pair(const Value *left, const Value *right, double *l, double *r)
{
    // Convert integer, float, and boolean operands to the common arithmetic type.
    if (left == NULL || right == NULL || l == NULL || r == NULL) {
        return 0;
    }

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

static Value operator_apply_arithmetic(OperatorType op, const Value *left, const Value *right)
{
    /* Preserve integer results when both operands are integers. Mixed numeric
       operands use double precision and return a float Value. */
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

    double lhs = 0.0;
    double rhs = 0.0;

    if (op == OPERATOR_ADD || op == OPERATOR_SUBTRACT || op == OPERATOR_MULTIPLY ||
        op == OPERATOR_DIVIDE || op == OPERATOR_MODULO) {
        if (!require_numeric_pair(left, right, &lhs, &rhs)) {
            fprintf(stderr, "Runtime Error: Arithmetic operators require numeric values.\n");
            return result;
        }

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

static int value_to_numeric(const Value *val, double *out)
{
    /* Relational comparisons may compare numeric values across types, including
       numeric strings and the language's infinity value. */
    if (val == NULL || out == NULL) return 0;
    if (val->type == VALUE_INTEGER) {
        *out = (double)val->integer_value;
        return 1;
    }
    if (val->type == VALUE_FLOAT) {
        *out = val->float_value;
        return 1;
    }
    if (val->type == VALUE_BOOLEAN) {
        *out = val->boolean_value ? 1.0 : 0.0;
        return 1;
    }
    if (val->type == VALUE_INFINITY) {
        *out = INFINITY;
        return 1;
    }
    if (val->type == VALUE_STRING && val->string_value != NULL) {
        char *end = NULL;
        double d = strtod(val->string_value, &end);
        if (end != val->string_value && *end == '\0') {
            *out = d;
            return 1;
        }
    }
    return 0;
}

static void value_to_string_buf(const Value *val, char *buf, size_t buf_size)
{
    // Creates a bounded textual representation for fallback comparisons.
    if (val == NULL || buf == NULL || buf_size == 0) return;
    buf[0] = '\0';
    if (val->type == VALUE_STRING) {
        snprintf(buf, buf_size, "%s", val->string_value ? val->string_value : "");
    } else if (val->type == VALUE_INTEGER) {
        snprintf(buf, buf_size, "%ld", val->integer_value);
    } else if (val->type == VALUE_FLOAT) {
        snprintf(buf, buf_size, "%g", val->float_value);
    } else if (val->type == VALUE_BOOLEAN) {
        snprintf(buf, buf_size, "%s", val->boolean_value ? "true" : "false");
    } else if (val->type == VALUE_NULL) {
        snprintf(buf, buf_size, "null");
    } else if (val->type == VALUE_INFINITY) {
        snprintf(buf, buf_size, "infinity");
    } else if (val->type == VALUE_ARRAY) {
        snprintf(buf, buf_size, "[array]");
    }
}

static Value operator_apply_relational(OperatorType op, const Value *left, const Value *right)
{
    /* Compare integers, strings, booleans, and compatible numeric values in
       that order; fallback values are compared using their text form. */
    Value result = value_create_boolean(0);

    if (left == NULL || right == NULL) {
        if (op == OPERATOR_NOT_EQUAL) {
            return value_create_boolean(left != right);
        }
        return value_create_boolean(left == right);
    }

    if (left->type == VALUE_INTEGER && right->type == VALUE_INTEGER) {
        long lval = left->integer_value;
        long rval = right->integer_value;
        if (op == OPERATOR_EQUAL_EQUAL) result = value_create_boolean(lval == rval);
        else if (op == OPERATOR_NOT_EQUAL) result = value_create_boolean(lval != rval);
        else if (op == OPERATOR_GREATER) result = value_create_boolean(lval > rval);
        else if (op == OPERATOR_LESS) result = value_create_boolean(lval < rval);
        else if (op == OPERATOR_GREATER_EQUAL) result = value_create_boolean(lval >= rval);
        else if (op == OPERATOR_LESS_EQUAL) result = value_create_boolean(lval <= rval);
        return result;
    }

    if (left->type == VALUE_STRING && right->type == VALUE_STRING) {
        const char *ls = left->string_value ? left->string_value : "";
        const char *rs = right->string_value ? right->string_value : "";
        int cmp = strcmp(ls, rs);
        if (op == OPERATOR_EQUAL_EQUAL) result = value_create_boolean(cmp == 0);
        else if (op == OPERATOR_NOT_EQUAL) result = value_create_boolean(cmp != 0);
        else if (op == OPERATOR_GREATER) result = value_create_boolean(cmp > 0);
        else if (op == OPERATOR_LESS) result = value_create_boolean(cmp < 0);
        else if (op == OPERATOR_GREATER_EQUAL) result = value_create_boolean(cmp >= 0);
        else if (op == OPERATOR_LESS_EQUAL) result = value_create_boolean(cmp <= 0);
        return result;
    }

    double lnum = 0.0, rnum = 0.0;
    if (value_to_numeric(left, &lnum) && value_to_numeric(right, &rnum)) {
        if (op == OPERATOR_EQUAL_EQUAL) result = value_create_boolean(lnum == rnum);
        else if (op == OPERATOR_NOT_EQUAL) result = value_create_boolean(lnum != rnum);
        else if (op == OPERATOR_GREATER) result = value_create_boolean(lnum > rnum);
        else if (op == OPERATOR_LESS) result = value_create_boolean(lnum < rnum);
        else if (op == OPERATOR_GREATER_EQUAL) result = value_create_boolean(lnum >= rnum);
        else if (op == OPERATOR_LESS_EQUAL) result = value_create_boolean(lnum <= rnum);
        return result;
    }

    char lstr[256], rstr[256];
    value_to_string_buf(left, lstr, sizeof(lstr));
    value_to_string_buf(right, rstr, sizeof(rstr));
    int cmp = strcmp(lstr, rstr);
    if (op == OPERATOR_EQUAL_EQUAL) result = value_create_boolean(cmp == 0);
    else if (op == OPERATOR_NOT_EQUAL) result = value_create_boolean(cmp != 0);
    else if (op == OPERATOR_GREATER) result = value_create_boolean(cmp > 0);
    else if (op == OPERATOR_LESS) result = value_create_boolean(cmp < 0);
    else if (op == OPERATOR_GREATER_EQUAL) result = value_create_boolean(cmp >= 0);
    else if (op == OPERATOR_LESS_EQUAL) result = value_create_boolean(cmp <= 0);

    return result;
}

static Value operator_apply_logical(OperatorType op, const Value *left, const Value *right)
{
    // Normalize supported operands to truth values before applying the connective.
    Value result = value_create_boolean(0);

    if (op == OPERATOR_NOT) {
        if (left == NULL) {
            fprintf(stderr, "Runtime Error: Logical NOT requires an operand.\n");
            return result;
        }
        int is_true = 0;
        if (left->type == VALUE_BOOLEAN) {
            is_true = left->boolean_value;
        } else if (left->type == VALUE_INTEGER) {
            is_true = left->integer_value != 0;
        } else if (left->type == VALUE_FLOAT) {
            is_true = left->float_value != 0.0;
        } else if (left->type == VALUE_STRING) {
            is_true = left->string_value != NULL && strlen(left->string_value) > 0;
        } else if (left->type == VALUE_NULL) {
            is_true = 0;
        }
        result = value_create_boolean(!is_true);
    } else if (op == OPERATOR_AND_AND) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Logical AND requires two operands.\n");
            return result;
        }
        int left_true = 0, right_true = 0;
        if (left->type == VALUE_BOOLEAN) left_true = left->boolean_value;
        else if (left->type == VALUE_INTEGER) left_true = left->integer_value != 0;
        else if (left->type == VALUE_FLOAT) left_true = left->float_value != 0.0;
        if (right->type == VALUE_BOOLEAN) right_true = right->boolean_value;
        else if (right->type == VALUE_INTEGER) right_true = right->integer_value != 0;
        else if (right->type == VALUE_FLOAT) right_true = right->float_value != 0.0;
        result = value_create_boolean(left_true && right_true);
    } else if (op == OPERATOR_OR_OR) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Logical OR requires two operands.\n");
            return result;
        }
        int left_true = 0, right_true = 0;
        if (left->type == VALUE_BOOLEAN) left_true = left->boolean_value;
        else if (left->type == VALUE_INTEGER) left_true = left->integer_value != 0;
        else if (left->type == VALUE_FLOAT) left_true = left->float_value != 0.0;
        if (right->type == VALUE_BOOLEAN) right_true = right->boolean_value;
        else if (right->type == VALUE_INTEGER) right_true = right->integer_value != 0;
        else if (right->type == VALUE_FLOAT) right_true = right->float_value != 0.0;
        result = value_create_boolean(left_true || right_true);
    }

    return result;
}

static Value operator_apply_bitwise(OperatorType op, const Value *left, const Value *right)
{
    /* NOT is unary. All other bitwise operators require two validated integer
       operands and return a newly constructed integer Value. */
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

static Value operator_apply_assignment(OperatorType op, const Value *left, const Value *right)
{
    /* This helper validates assignment operands but does not mutate bindings;
       the interpreter performs the actual environment update. */
    Value result = value_create_null();

    if (op == OPERATOR_ADD_ASSIGN || op == OPERATOR_SUBTRACT_ASSIGN ||
        op == OPERATOR_MULTIPLY_ASSIGN || op == OPERATOR_DIVIDE_ASSIGN ||
        op == OPERATOR_MODULO_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Assignment operators require operands.\n");
            return result;
        }
        result = *left;
    } else if (op == OPERATOR_AND_ASSIGN || op == OPERATOR_OR_ASSIGN ||
               op == OPERATOR_XOR_ASSIGN || op == OPERATOR_SHIFT_LEFT_ASSIGN ||
               op == OPERATOR_SHIFT_RIGHT_ASSIGN) {
        if (left == NULL || right == NULL) {
            fprintf(stderr, "Runtime Error: Bitwise assignment operators require operands.\n");
            return result;
        }
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

static Value operator_apply_increment(OperatorType op, const Value *value)
{
    // Increment and decrement calculate replacement values without mutating the operand.
    Value result = value_create_null();

    if (value == NULL) {
        fprintf(stderr, "Runtime Error: Increment/decrement operators require an operand.\n");
        return result;
    }

    if (op == OPERATOR_INCREMENT) {
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

static Value operator_apply_range(OperatorType op, const Value *start, const Value *end)
{
    /* Build an owned dynamic integer array, choosing ascending or descending
       order from the two inclusive endpoints. */
    Value result = value_create_null();

    if (op == OPERATOR_RANGE) {
        if (start == NULL || end == NULL) {
            fprintf(stderr, "Runtime Error: Range operator requires start and end values.\n");
            return result;
        }

        if (start->type == VALUE_INTEGER && end->type == VALUE_INTEGER) {
            long start_val = start->integer_value;
            long end_val = end->integer_value;
            size_t count = 0;
            Value *elements = NULL;

            if (start_val <= end_val) {
                count = (size_t)(end_val - start_val + 1);
                elements = (Value *)malloc(count * sizeof(Value));
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start_val + (long)i);
                    }
                }
            } else {
                count = (size_t)(start_val - end_val + 1);
                elements = (Value *)malloc(count * sizeof(Value));
                if (elements != NULL) {
                    for (size_t i = 0; i < count; i++) {
                        elements[i] = value_create_integer(start_val - (long)i);
                    }
                }
            }

            if (elements != NULL) {
                ArrayValue *arr = array_value_create_dynamic_from_values(VALUE_INTEGER, elements, count);
                if (arr != NULL) {
                    result = value_create_array(arr);
                }
                for (size_t i = 0; i < count; i++) {
                    value_free(&elements[i]);
                }
                free(elements);
            } else {
                fprintf(stderr, "Runtime Error: Out of memory creating range.\n");
            }
        } else {
            fprintf(stderr, "Runtime Error: Range operator requires integer operands.\n");
        }
    }

    return result;
}

static Value operator_apply_membership(OperatorType op, const Value *container, const Value *member)
{
    /* Search arrays by compatible scalar type and strings by substring. A
       boolean false is retained for unsupported or absent members. */
    Value result = value_create_boolean(0);

    if (op == OPERATOR_IN) {
        if (container == NULL) {
            fprintf(stderr, "Runtime Error: Membership operator requires a container.\n");
            return result;
        }

        if (container->type == VALUE_ARRAY && member != NULL) {
            ArrayValue *arr = container->array_value;
            if (arr != NULL) {
                for (size_t i = 0; i < arr->length; i++) {
                    Value *elem = &arr->elements[i];

                    if (member->type == VALUE_INTEGER && elem->type == VALUE_INTEGER) {
                        if (member->integer_value == elem->integer_value) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_STRING && elem->type == VALUE_STRING) {
                        const char *member_str = member->string_value ? member->string_value : "";
                        const char *elem_str = elem->string_value ? elem->string_value : "";
                        if (strcmp(member_str, elem_str) == 0) {
                            return value_create_boolean(1);
                        }
                    } else if (member->type == VALUE_FLOAT && elem->type == VALUE_FLOAT) {
                        if (member->float_value == elem->float_value) {
                            return value_create_boolean(1);
                        }
                    }
                }
            }
        } else if (container->type == VALUE_STRING && member != NULL) {
            if (member->type == VALUE_STRING) {
                const char *container_str = container->string_value ? container->string_value : "";
                const char *member_str = member->string_value ? member->string_value : "";
                if (strstr(container_str, member_str) != NULL) {
                    return value_create_boolean(1);
                }
            }
        }
    }

    return result;
}
