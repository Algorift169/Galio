/* SPDX-License-Identifier: AGPL-3.0-only */
/* Values centralize type tags, deep copies, and destruction so
 arrays and strings have one ownership policy. Nothing else should free 
or copy these payloads directly. ANd the Value structure is used to 
represent all types of values in the Drift programming language, including integers, 
floats, strings, booleans, null, infinity, and arrays. Each Value instance contains 
a type tag indicating the kind of value it represents, along with 
the corresponding payload for that type. The Value structure provides a 
consistent way to manage and manipulate values in the interpreter,
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array_value.h"
#include "drift/value.h"

/* Builds an integer value and clears unrelated payload fields for predictable ownership. */
Value value_create_integer(long value)
{
    Value result;
    result.type = VALUE_INTEGER;
    result.integer_value = value;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.identity = result.string_value;
    return result;
}

/* Builds a floating-point value while leaving string and array ownership empty. */
Value value_create_float(double value)
{
    Value result;
    result.type = VALUE_FLOAT;
    result.integer_value = 0;
    result.float_value = value;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.identity = NULL;
    return result;
}

/* Allocates a private copy of string text so callers may release their input independently. */
Value value_create_string(const char *value)
{
    Value result;
    result.type = VALUE_STRING;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.boolean_value = 0;
    result.string_value = NULL;

    if (value != NULL) {
        size_t length = strlen(value); // Measure the source before allocating its terminator.
        result.string_value = (char *)malloc(length + 1U);
        if (result.string_value != NULL) {
            memcpy(result.string_value, value, length + 1U);
        }
    }

    result.identity = result.string_value;

    return result;
}

/* Stores the language boolean payload without borrowing memory from the caller. */
Value value_create_boolean(int value)
{
    Value result;
    result.type = VALUE_BOOLEAN;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = value;
    result.identity = NULL;
    return result;
}

/* Returns the sentinel value used when no scalar payload is present. */
Value value_create_null(void)
{
    Value result;
    result.type = VALUE_NULL;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.identity = NULL;
    return result;
}

/* Represents positive infinity using the host floating-point constant. */
Value value_create_infinity(void)
{
    Value result;
    result.type = VALUE_INFINITY;
    result.integer_value = 0;
    result.float_value = INFINITY;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.identity = NULL;
    return result;
}

/* Wraps an array pointer and records it as the identity used by comparisons. */
Value value_create_array(ArrayValue *value)
{
    Value result;
    result.type = VALUE_ARRAY;
    result.integer_value = 0;
    result.float_value = 0.0;
    result.string_value = NULL;
    result.boolean_value = 0;
    result.array_value = value;
    result.identity = value;
    return result;
}

/* Deep-copies owned strings and arrays while preserving scalar type and identity rules. */
Value value_copy(const Value *value)
{
    Value result;

    if (value == NULL) {
        return value_create_string(NULL);
    }

    // Select the copy routine from the active tag so only the relevant payload is duplicated.
    switch (value->type) {
        case VALUE_INTEGER:
            result = value_create_integer(value->integer_value);
            break;
        case VALUE_FLOAT:
            result = value_create_float(value->float_value);
            break;
        case VALUE_STRING:
            result = value_create_string(value->string_value);
            result.identity = value->identity;
            break;
        case VALUE_BOOLEAN:
            result = value_create_boolean(value->boolean_value);
            break;
        case VALUE_NULL:
            result = value_create_null();
            break;
        case VALUE_INFINITY:
            result = value_create_infinity();
            break;
        case VALUE_ARRAY:
            result.type = VALUE_ARRAY;
            result.array_value = array_value_copy(value->array_value);
            result.integer_value = 0;
            result.float_value = 0.0;
            result.string_value = NULL;
            result.boolean_value = 0;
            result.identity = value->identity;
            break;
        default:
            result = value_create_string(NULL);
            break;
    }

    return result;
}

/* Releases every payload owned by value, then resets it to a reusable null state. */
void value_free(Value *value)
{
    if (value == NULL) {
        return;
    }

    if (value->type == VALUE_ARRAY) { // Arrays own nested values and must be freed recursively.
        array_value_free(value->array_value);
        value->array_value = NULL;
    }

    free(value->string_value);
    value->string_value = NULL;
    value->type = VALUE_NULL;
    value->integer_value = 0;
    value->float_value = 0.0;
    value->boolean_value = 0;
    value->identity = NULL;
}
