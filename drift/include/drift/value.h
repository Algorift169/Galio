/* SPDX-License-Identifier: AGPL-3.0-only */
/* Value tags define scalar and array payload ownership shared by every evaluator. */

#ifndef DRIFT_VALUE_H
#define DRIFT_VALUE_H

typedef struct ArrayValue ArrayValue;

typedef enum ValueType {
    VALUE_INTEGER,
    VALUE_FLOAT,
    VALUE_STRING,
    VALUE_BOOLEAN,
    VALUE_NULL,
    VALUE_INFINITY,
    VALUE_ARRAY
} ValueType;

typedef struct Value {
    ValueType type;
    long integer_value;
    double float_value;
    char *string_value;
    int boolean_value;
    ArrayValue *array_value;
    const void *identity;
} Value;

/* Creates a scalar Value with the requested integer payload. */
Value value_create_integer(long value);
/* Creates a scalar Value with the requested floating-point payload. */
Value value_create_float(double value);
/* Copies string text into a Value-owned allocation. */
Value value_create_string(const char *value);
/* Normalizes a truth value into the language boolean representation. */
Value value_create_boolean(int value);
/* Creates the language null value without an associated payload. */
Value value_create_null(void);
/* Creates the distinguished infinity value used by the evaluator. */
Value value_create_infinity(void);
/* Wraps an existing array object in a Value without changing its contents. */
Value value_create_array(ArrayValue *value);
/* Copies the active payload so the result can be released independently. */
Value value_copy(const Value *value);
/* Releases allocations owned by a Value and clears its managed payload. */
void value_free(Value *value);

#endif
