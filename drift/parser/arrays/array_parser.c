/* SPDX-License-Identifier: AGPL-3.0-only */
/* Array parsing records dimensions before elements so flat storage and index arity remain consistent. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/parser.h"
#include "drift/value.h"
#include "drift/array_value.h"

static Token *parser_peek(Parser *parser);
static Token *parser_advance(Parser *parser);

static int parse_dynamic_initializer_values(Parser *parser, ValueType *element_type, Value **out_values, size_t *out_count);
static int parse_dynamic_initializer_level(Parser *parser,
                                           size_t level,
                                           size_t dimension_count,
                                           const long *declared_dimensions,
                                           long *out_dimensions,
                                           ValueType *element_type,
                                           Value **out_values,
                                           size_t *out_count);
static int parse_dynamic_multi_dimensional_initializer(Parser *parser,
                                                      size_t dimension_count,
                                                      const long *declared_dimensions,
                                                      long *out_dimensions,
                                                      ValueType *element_type,
                                                      Value **out_values,
                                                      size_t *out_count);

/*
Reads one integer token and converts its text into a long value. Keeping this
conversion in one helper lets dimension and index parsing use the same token
validation rules.
*/
static int parse_integer_token(Token *token, long *out_value)
{
    if (token == NULL || token->type != TOKEN_INTEGER) {
        return 0;
    }

    char *end = NULL;
    long value = strtol(token->value, &end, 10);
    (void)end;
    *out_value = value;
    return 1;
}

/*
Checks the current token and consumes it only when it has the expected type.
The caller supplies the user-facing error because the same operation is used
for different grammar rules, such as closing brackets and braces.
*/
static int parser_expect(Parser *parser, TokenType type, const char *message)
{
    Token *token = parser_peek(parser);
    if (token == NULL || token->type != type) {
        if (message != NULL) {
            fprintf(stderr, "%s\n", message);
        }
        return 0;
    }
    parser_advance(parser);
    return 1;
}

/*
Identifies the literal tokens allowed inside array initializers. Identifiers
and expressions are intentionally excluded because array elements are parsed
as literal values in this grammar.
*/
static int is_literal_value_token(Token *token)
{
    return token != NULL && (token->type == TOKEN_INTEGER || token->type == TOKEN_FLOAT || token->type == TOKEN_STRING || token->type == TOKEN_TRUE || token->type == TOKEN_FALSE || token->type == TOKEN_NULL || token->type == TOKEN_INFINITY);
}

/*
Converts a literal token into a newly created runtime Value. Quoted strings are
copied without their delimiters; all other supported literal kinds are mapped
directly to their corresponding Value constructor.
*/
static Value parse_literal_value_token(Token *token)
{
    if (token == NULL) {
        return value_create_string(NULL);
    }

    if (token->type == TOKEN_INTEGER) {
        char *end = NULL;
        long value = strtol(token->value, &end, 10);
        (void)end;
        return value_create_integer(value);
    }

    if (token->type == TOKEN_FLOAT) {
        char *end = NULL;
        double value = strtod(token->value, &end);
        (void)end;
        return value_create_float(value);
    }

    if (token->type == TOKEN_STRING) {
        if (token->value != NULL && token->value[0] == '\'' && token->value[strlen(token->value) - 1] == '\'') {
            size_t length = strlen(token->value) - 2U;
            char *stripped = (char *)malloc(length + 1U);
            if (stripped != NULL) {
                memcpy(stripped, token->value + 1, length);
                stripped[length] = '\0';
                Value value = value_create_string(stripped);
                free(stripped);
                return value;
            }
        }
        return value_create_string(token->value);
    }

    if (token->type == TOKEN_TRUE) {
        return value_create_boolean(1);
    }

    if (token->type == TOKEN_FALSE) {
        return value_create_boolean(0);
    }

    if (token->type == TOKEN_NULL) {
        return value_create_null();
    }

    if (token->type == TOKEN_INFINITY) {
        return value_create_infinity();
    }

    return value_create_string(NULL);
}

/*
Defines which element types can coexist in one array. Integer and float values
are compatible with each other, while all other types must match exactly.
*/
static int array_element_type_compatible(ValueType current, ValueType incoming)
{
    if (current == incoming) {
        return 1;
    }

    if ((current == VALUE_INTEGER || current == VALUE_FLOAT) &&
        (incoming == VALUE_INTEGER || incoming == VALUE_FLOAT)) {
        return 1;
    }

    return 0;
}

/*
Updates the inferred element type as values are encountered. A leading null
does not decide the type, but infinity cannot be combined with another type.
*/
static int array_update_element_type(ValueType *type, ValueType incoming)
{
    if (*type == VALUE_NULL && incoming != VALUE_NULL) {
        *type = incoming;
        return 1;
    }
    if (*type == VALUE_INFINITY && incoming != VALUE_INFINITY) {
        return 0;
    }
    return array_element_type_compatible(*type, incoming);
}

/*
Parses one fixed-shape brace level. At the deepest level it collects literal
values; at higher levels it recursively collects child braces and flattens
their values into the order used by ArrayValue.
*/
static int parse_initializer_values(Parser *parser,
                                    size_t level,
                                    size_t dimension_count,
                                    const long *dimensions,
                                    ValueType *element_type,
                                    Value **out_values,
                                    size_t *out_count)
{
    Token *token = parser_peek(parser);
    if (token == NULL) {
        fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
        return 0;
    }

    if (token->type != TOKEN_LEFT_BRACE) {
        fprintf(stderr, "Syntax Error: Expected '{' in array initializer.\n");
        return 0;
    }
    parser_advance(parser);

    // Values are stored flat even though the source initializer is nested.
    Value *values = NULL;
    size_t count = 0;
    size_t expected_children = (level < dimension_count ? (size_t)dimensions[level] : 0);
    size_t child_count = 0;

    while (1) {
        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (level + 1 == dimension_count) {
            // The deepest level contains actual literals rather than child arrays.
            if (!is_literal_value_token(token)) {
                fprintf(stderr, "Syntax Error: Expected literal value in array initializer.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value value = parse_literal_value_token(token);
            if (count == 0) {
                *element_type = value.type;
            } else if (!array_update_element_type(element_type, value.type)) {
                value_free(&value);
                fprintf(stderr, "Type Error: Array elements must all have the same data type.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            parser_advance(parser);
            Value *new_values = (Value *)realloc(values, (count + 1U) * sizeof(Value));
            if (new_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                value_free(&value);
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }
            values = new_values;
            values[count++] = value;
        } else {
            // Higher levels recurse into each child brace, then append its flat values.
            if (token->type != TOKEN_LEFT_BRACE) {
                fprintf(stderr, "Syntax Error: Expected '{' in nested array initializer.\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value *child_values = NULL;
            size_t child_count_local = 0;
            if (!parse_initializer_values(parser, level + 1, dimension_count, dimensions, element_type, &child_values, &child_count_local)) {
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }

            Value *new_values = (Value *)realloc(values, (count + child_count_local) * sizeof(Value));
            if (new_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                for (size_t i = 0; i < child_count_local; ++i) {
                    value_free(&child_values[i]);
                }
                free(child_values);
                return 0;
            }

            values = new_values;
            for (size_t i = 0; i < child_count_local; ++i) {
                values[count + i] = child_values[i];
            }
            count += child_count_local;
            free(child_values);
            child_count++;
        }

        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (token->type == TOKEN_LEFT_BRACE && level + 1 < dimension_count && child_count < expected_children) {
            continue;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or '}' in array initializer.\n");
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_BRACE, "Syntax Error: Unterminated array initializer.")) {
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

/*
Reads consecutive bracket dimensions such as [2][3]. An empty bracket stores
zero as a placeholder and marks the declaration dynamic; fixed dimensions keep
their declared sizes for later initializer validation.
*/
static int parse_array_dimensions(Parser *parser, long **out_dimensions, size_t *out_count, int *error, int *out_dynamic_declaration)
{
    size_t count = 0;
    long *dimensions = NULL;
    int dynamic_declaration = 0;

    while (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);

        Token *token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_RIGHT_BRACKET) {
            // [] means that this dimension will be inferred from its initializer.
            parser_advance(parser);
            dynamic_declaration = 1;

            long *new_dimensions = (long *)realloc(dimensions, (count + 1U) * sizeof(long));
            if (new_dimensions == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array dimensions\n");
                free(dimensions);
                *error = 1;
                return 0;
            }
            dimensions = new_dimensions;
            dimensions[count++] = 0;
            continue;
        }

        if (token == NULL || token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected array size after '['.\n");
            free(dimensions);
            *error = 1;
            return 0;
        }

        long size = 0;
        if (!parse_integer_token(token, &size)) {
            fprintf(stderr, "Syntax Error: Invalid array size.\n");
            free(dimensions);
            *error = 1;
            return 0;
        }

        parser_advance(parser);
        // The closing bracket is required before this dimension can be recorded.
        if (!parser_expect(parser, TOKEN_RIGHT_BRACKET, "Syntax Error: Expected ']' after array size.")) {
            free(dimensions);
            *error = 1;
            return 0;
        }

        long *new_dimensions = (long *)realloc(dimensions, (count + 1U) * sizeof(long));
        if (new_dimensions == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array dimensions\n");
            free(dimensions);
            *error = 1;
            return 0;
        }
        dimensions = new_dimensions;
        dimensions[count++] = size;
    }

    *out_dimensions = dimensions;
    *out_count = count;
    if (out_dynamic_declaration != NULL) {
        *out_dynamic_declaration = dynamic_declaration;
    }
    return 1;
}

/*
Parses the outer blocks of a fixed multidimensional initializer. Each block is
flattened by parse_initializer_values and appended to one contiguous Value
buffer, matching the array module's row-major storage layout.
*/
static int parse_multi_dimensional_initializer(Parser *parser,
                                              size_t dimension_count,
                                              const long *dimensions,
                                              ValueType *element_type,
                                              Value **out_values,
                                              size_t *out_count)
{
    Value *values = NULL;
    size_t count = 0;

    // The first declared dimension tells us how many top-level blocks to read.
    for (long block = 0; block < dimensions[0]; ++block) {
        Value *block_values = NULL;
        size_t block_count = 0;
        if (!parse_initializer_values(parser, 1, dimension_count, dimensions, element_type, &block_values, &block_count)) {
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        Value *new_values = (Value *)realloc(values, (count + block_count) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < block_count; ++i) {
                value_free(&block_values[i]);
            }
            free(block_values);
            return 0;
        }

        values = new_values;
        for (size_t i = 0; i < block_count; ++i) {
            values[count + i] = block_values[i];
        }
        count += block_count;
        free(block_values);
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

static int parse_dynamic_initializer_level(Parser *parser,
                                           size_t level,
                                           size_t dimension_count,
                                           const long *declared_dimensions,
                                           long *out_dimensions,
                                           ValueType *element_type,
                                           Value **out_values,
                                           size_t *out_count)
{
    /*
    Dynamic parsing discovers dimensions from the source. Fixed dimensions in
    declared_dimensions still act as constraints, while zero dimensions are
    replaced by the sizes found in each brace level.
    */
    if (!parser_expect(parser, TOKEN_LEFT_BRACE, "Syntax Error: Expected '{' in dynamic array initializer.")) {
        return 0;
    }

    if (level + 1 == dimension_count) {
        // At the leaf level, infer the dimension from the number of literals.
        if (!parse_dynamic_initializer_values(parser, element_type, out_values, out_count)) {
            return 0;
        }

        if (declared_dimensions[level] != 0 && *out_count != (size_t)declared_dimensions[level]) {
            fprintf(stderr, "Syntax Error: Expected %ld elements but found %zu.\n", declared_dimensions[level], *out_count);
            for (size_t i = 0; i < *out_count; ++i) {
                value_free(&(*out_values)[i]);
            }
            free(*out_values);
            return 0;
        }

        out_dimensions[level] = (long)*out_count;
        return 1;
    }

    Value *values = NULL;
    size_t count = 0;
    size_t child_count = 0;
    long *max_child_dimensions = NULL;
    size_t *child_value_counts = NULL;
    Value **child_values_array = NULL;
    long **child_dimensions_array = NULL;
    size_t child_dim_count = dimension_count - (level + 1);

    // Keep each child temporarily so ragged children can be compared and padded.
    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < child_count; ++i) {
                for (size_t j = 0; j < child_value_counts[i]; ++j) {
                    value_free(&child_values_array[i][j]);
                }
                free(child_values_array[i]);
                free(child_dimensions_array[i]);
            }
            free(child_values_array);
            free(child_value_counts);
            free(child_dimensions_array);
            free(max_child_dimensions);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (token->type != TOKEN_LEFT_BRACE) {
            fprintf(stderr, "Syntax Error: Expected '{' in nested array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < child_count; ++i) {
                for (size_t j = 0; j < child_value_counts[i]; ++j) {
                    value_free(&child_values_array[i][j]);
                }
                free(child_values_array[i]);
                free(child_dimensions_array[i]);
            }
            free(child_values_array);
            free(child_value_counts);
            free(child_dimensions_array);
            free(max_child_dimensions);
            return 0;
        }

        long *child_dimensions = (long *)calloc(dimension_count, sizeof(long));
        if (child_dimensions == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < child_count; ++i) {
                for (size_t j = 0; j < child_value_counts[i]; ++j) {
                    value_free(&child_values_array[i][j]);
                }
                free(child_values_array[i]);
                free(child_dimensions_array[i]);
            }
            free(child_values_array);
            free(child_value_counts);
            free(child_dimensions_array);
            free(max_child_dimensions);
            return 0;
        }

        Value *child_values = NULL;
        size_t child_value_count = 0;
        if (!parse_dynamic_initializer_level(parser, level + 1, dimension_count, declared_dimensions, child_dimensions, element_type, &child_values, &child_value_count)) {
            free(child_dimensions);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            for (size_t i = 0; i < child_count; ++i) {
                for (size_t j = 0; j < child_value_counts[i]; ++j) {
                    value_free(&child_values_array[i][j]);
                }
                free(child_values_array[i]);
                free(child_dimensions_array[i]);
            }
            free(child_values_array);
            free(child_value_counts);
            free(child_dimensions_array);
            free(max_child_dimensions);
            return 0;
        }

        if (child_count == 0) {
            // The first child establishes the reference shape for its siblings.
            max_child_dimensions = (long *)malloc(child_dim_count * sizeof(long));
            if (max_child_dimensions == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t i = 0; i < child_value_count; ++i) {
                    value_free(&child_values[i]);
                }
                free(child_values);
                free(child_dimensions);
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                for (size_t i = 0; i < child_count; ++i) {
                    for (size_t j = 0; j < child_value_counts[i]; ++j) {
                        value_free(&child_values_array[i][j]);
                    }
                    free(child_values_array[i]);
                    free(child_dimensions_array[i]);
                }
                free(child_values_array);
                free(child_value_counts);
                free(child_dimensions_array);
                return 0;
            }
            for (size_t i = 0; i < child_dim_count; ++i) {
                max_child_dimensions[i] = child_dimensions[level + 1 + i];
            }
        } else {
            // Fixed child dimensions must match; dynamic dimensions expand to the largest child.
            for (size_t i = 0; i < child_dim_count; ++i) {
                long declared_dim = declared_dimensions[level + 1 + i];
                long child_dim = child_dimensions[level + 1 + i];
                if (declared_dim != 0) {
                    if (child_dim != declared_dim) {
                        fprintf(stderr, "Syntax Error: Expected %ld elements but found %ld.\n", declared_dim, child_dim);
                        for (size_t j = 0; j < child_value_count; ++j) {
                            value_free(&child_values[j]);
                        }
                        free(child_values);
                        free(child_dimensions);
                        for (size_t k = 0; k < child_count; ++k) {
                            for (size_t j = 0; j < child_value_counts[k]; ++j) {
                                value_free(&child_values_array[k][j]);
                            }
                            free(child_values_array[k]);
                            free(child_dimensions_array[k]);
                        }
                        free(child_values_array);
                        free(child_value_counts);
                        free(child_dimensions_array);
                        free(max_child_dimensions);
                        return 0;
                    }
                } else if (child_dim > max_child_dimensions[i]) {
                    max_child_dimensions[i] = child_dim;
                }
            }
        }

        Value **new_child_values_array = (Value **)realloc(child_values_array, (child_count + 1U) * sizeof(Value *));
        size_t *new_child_value_counts = (size_t *)realloc(child_value_counts, (child_count + 1U) * sizeof(size_t));
        long **new_child_dimensions_array = (long **)realloc(child_dimensions_array, (child_count + 1U) * sizeof(long *));
        if (new_child_values_array == NULL || new_child_value_counts == NULL || new_child_dimensions_array == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t j = 0; j < child_value_count; ++j) {
                value_free(&child_values[j]);
            }
            free(child_values);
            free(child_dimensions);
            for (size_t k = 0; k < child_count; ++k) {
                for (size_t j = 0; j < child_value_counts[k]; ++j) {
                    value_free(&child_values_array[k][j]);
                }
                free(child_values_array[k]);
                free(child_dimensions_array[k]);
            }
            free(child_values_array);
            free(child_value_counts);
            free(child_dimensions_array);
            free(max_child_dimensions);
            return 0;
        }

        child_values_array = new_child_values_array;
        child_value_counts = new_child_value_counts;
        child_dimensions_array = new_child_dimensions_array;

        child_values_array[child_count] = child_values;
        child_value_counts[child_count] = child_value_count;
        child_dimensions_array[child_count] = child_dimensions;
        child_count++;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token != NULL && token->type == TOKEN_RIGHT_BRACE) {
            continue;
        }

        if (token != NULL && token->type == TOKEN_LEFT_BRACE) {
            continue;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or '}' in array initializer.\n");
        for (size_t i = 0; i < child_count; ++i) {
            for (size_t j = 0; j < child_value_counts[i]; ++j) {
                value_free(&child_values_array[i][j]);
            }
            free(child_values_array[i]);
            free(child_dimensions_array[i]);
        }
        free(child_values_array);
        free(child_value_counts);
        free(child_dimensions_array);
        free(max_child_dimensions);
        return 0;
    }

    if (child_count == 0) {
        if (!parser_expect(parser, TOKEN_RIGHT_BRACE, "Syntax Error: Unterminated array initializer.")) {
            return 0;
        }
        if (declared_dimensions[level] != 0 && child_count != (size_t)declared_dimensions[level]) {
            fprintf(stderr, "Syntax Error: Expected %ld elements but found %zu.\n", declared_dimensions[level], child_count);
            return 0;
        }
        out_dimensions[level] = (long)child_count;
        for (size_t i = 0; i < child_dim_count; ++i) {
            out_dimensions[level + 1 + i] = declared_dimensions[level + 1 + i];
        }
        *out_values = NULL;
        *out_count = 0;
        free(max_child_dimensions);
        return 1;
    }

    size_t max_child_total_count = 1;
    for (size_t i = 0; i < child_dim_count; ++i) {
        if (max_child_dimensions[i] < 0) {
            max_child_total_count = 0;
            break;
        }
        max_child_total_count *= (size_t)max_child_dimensions[i];
    }

    for (size_t i = 0; i < child_count; ++i) {
        if (child_value_counts[i] < max_child_total_count) {
            Value *padded_values = (Value *)malloc(max_child_total_count * sizeof(Value));
            if (padded_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t k = 0; k < child_count; ++k) {
                    for (size_t j = 0; j < child_value_counts[k]; ++j) {
                        value_free(&child_values_array[k][j]);
                    }
                    free(child_values_array[k]);
                    free(child_dimensions_array[k]);
                }
                free(child_values_array);
                free(child_value_counts);
                free(child_dimensions_array);
                free(max_child_dimensions);
                return 0;
            }
            for (size_t j = 0; j < child_value_counts[i]; ++j) {
                padded_values[j] = child_values_array[i][j];
            }
            for (size_t j = child_value_counts[i]; j < max_child_total_count; ++j) {
                padded_values[j] = value_create_null();
            }
            free(child_values_array[i]);
            child_values_array[i] = padded_values;
            child_value_counts[i] = max_child_total_count;
        }
    }

    size_t total_count = child_count * max_child_total_count;
    values = (Value *)malloc(total_count * sizeof(Value));
    if (values == NULL) {
        fprintf(stderr, "Error: out of memory while parsing array initializer\n");
        for (size_t k = 0; k < child_count; ++k) {
            for (size_t j = 0; j < child_value_counts[k]; ++j) {
                value_free(&child_values_array[k][j]);
            }
            free(child_values_array[k]);
            free(child_dimensions_array[k]);
        }
        free(child_values_array);
        free(child_value_counts);
        free(child_dimensions_array);
        free(max_child_dimensions);
        return 0;
    }

    for (size_t i = 0; i < child_count; ++i) {
        memcpy(values + i * max_child_total_count, child_values_array[i], max_child_total_count * sizeof(Value));
        free(child_values_array[i]);
        free(child_dimensions_array[i]);
    }
    free(child_values_array);
    free(child_value_counts);
    free(child_dimensions_array);

    if (!parser_expect(parser, TOKEN_RIGHT_BRACE, "Syntax Error: Unterminated array initializer.")) {
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        free(max_child_dimensions);
        return 0;
    }

    if (declared_dimensions[level] != 0 && child_count != (size_t)declared_dimensions[level]) {
        fprintf(stderr, "Syntax Error: Expected %ld elements but found %zu.\n", declared_dimensions[level], child_count);
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        free(max_child_dimensions);
        return 0;
    }

    out_dimensions[level] = (long)child_count;
    for (size_t i = 0; i < child_dim_count; ++i) {
        out_dimensions[level + 1 + i] = max_child_dimensions[i];
    }
    *out_values = values;
    *out_count = total_count;
    free(max_child_dimensions);
    return 1;
}

static int parse_dynamic_multi_dimensional_initializer(Parser *parser,
                                                      size_t dimension_count,
                                                      const long *declared_dimensions,
                                                      long *out_dimensions,
                                                      ValueType *element_type,
                                                      Value **out_values,
                                                      size_t *out_count)
{
    /*
    Parses dynamic arrays with more than one dimension. Each top-level block is
    parsed independently first, then all blocks are normalized to one common
    shape so the final flat buffer can be indexed like a rectangular array.
    */
    long *child_dimensions = NULL;
    Value *values = NULL;
    size_t count = 0;
    size_t block_count = 0;
    long *expected_child_dimensions = NULL;
    size_t expected_child_total_count = 0;
    size_t child_dim_count = dimension_count - 1;
    int use_outer_wrapper = 0;

    Token *first = parser_peek(parser);
    if (first == NULL || first->type != TOKEN_LEFT_BRACE) {
        fprintf(stderr, "Syntax Error: Expected '{' in array initializer.\n");
        return 0;
    }

    size_t opening_brace_count = 0;
    // Accept either an outer wrapper around all blocks or adjacent top-level blocks.
    while (parser->index + opening_brace_count < parser->count &&
           parser->tokens[parser->index + opening_brace_count].type == TOKEN_LEFT_BRACE) {
        opening_brace_count++;
    }
    if (opening_brace_count >= dimension_count) {
        use_outer_wrapper = 1;
        parser_advance(parser);
    }

    child_dimensions = (long *)malloc(dimension_count * sizeof(long));
    if (child_dimensions == NULL) {
        fprintf(stderr, "Error: out of memory while parsing array initializer\n");
        return 0;
    }

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            free(expected_child_dimensions);
            return 0;
        }

        if (use_outer_wrapper && token->type == TOKEN_RIGHT_BRACE) {
            break;
        }

        if (token->type != TOKEN_LEFT_BRACE) {
            if (block_count == 0) {
                fprintf(stderr, "Syntax Error: Expected '{' in array initializer.\n");
            } else {
                fprintf(stderr, "Syntax Error: Expected additional array block or closing brace.\n");
            }
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            free(expected_child_dimensions);
            return 0;
        }

        child_dimensions = (long *)malloc(dimension_count * sizeof(long));
        if (child_dimensions == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            free(expected_child_dimensions);
            return 0;
        }

        Value *child_values = NULL;
        size_t child_value_count = 0;
        if (!parse_dynamic_initializer_level(parser, 1, dimension_count, declared_dimensions, child_dimensions, element_type, &child_values, &child_value_count)) {
            free(child_dimensions);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            free(expected_child_dimensions);
            return 0;
        }

        if (block_count == 0) {
            // The first block provides the initial shape for later blocks.
            expected_child_dimensions = (long *)malloc(child_dim_count * sizeof(long));
            if (expected_child_dimensions == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t i = 0; i < child_value_count; ++i) {
                    value_free(&child_values[i]);
                }
                free(child_values);
                free(child_dimensions);
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }
            memcpy(expected_child_dimensions, child_dimensions + 1, child_dim_count * sizeof(long));
            memcpy(&out_dimensions[1], child_dimensions + 1, child_dim_count * sizeof(long));
        } else {
            // Fixed dimensions must agree; dynamic dimensions grow when a later block is larger.
            for (size_t i = 0; i < child_dim_count; ++i) {
                if (declared_dimensions[i + 1] != 0 &&
                    child_dimensions[i + 1] != declared_dimensions[i + 1]) {
                    fprintf(stderr, "Syntax Error: Expected %ld elements but found %ld.\n",
                            declared_dimensions[i + 1], child_dimensions[i + 1]);
                    for (size_t j = 0; j < child_value_count; ++j) {
                        value_free(&child_values[j]);
                    }
                    free(child_values);
                    free(child_dimensions);
                    free(expected_child_dimensions);
                    for (size_t j = 0; j < count; ++j) {
                        value_free(&values[j]);
                    }
                    free(values);
                    return 0;
                }
                if (declared_dimensions[i + 1] == 0 &&
                    child_dimensions[i + 1] > expected_child_dimensions[i]) {
                    expected_child_dimensions[i] = child_dimensions[i + 1];
                    out_dimensions[i + 1] = expected_child_dimensions[i];
                }
            }
        }

        size_t new_child_total_count = 1;
        for (size_t i = 0; i < child_dim_count; ++i) {
            new_child_total_count *= (size_t)expected_child_dimensions[i];
        }

        if (expected_child_total_count != 0 && new_child_total_count > expected_child_total_count) {
            // When the shape grows, expand earlier blocks and fill new cells with null.
            Value *resized_values = (Value *)realloc(values, block_count * new_child_total_count * sizeof(Value));
            if (resized_values == NULL) {
                fprintf(stderr, "Error: out of memory while parsing array initializer\n");
                for (size_t i = 0; i < child_value_count; ++i) {
                    value_free(&child_values[i]);
                }
                free(child_values);
                free(child_dimensions);
                free(expected_child_dimensions);
                for (size_t i = 0; i < count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                return 0;
            }
            values = resized_values;
            for (size_t i = block_count; i > 0; --i) {
                size_t offset = i - 1;
                memmove(values + offset * new_child_total_count,
                        values + offset * expected_child_total_count,
                        expected_child_total_count * sizeof(Value));
                for (size_t j = expected_child_total_count; j < new_child_total_count; ++j) {
                    values[offset * new_child_total_count + j] = value_create_null();
                }
            }
            count = block_count * new_child_total_count;
        }
        expected_child_total_count = new_child_total_count;

        Value *new_values = (Value *)realloc(values, (count + expected_child_total_count) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            for (size_t i = 0; i < child_value_count; ++i) {
                value_free(&child_values[i]);
            }
            free(child_values);
            free(child_dimensions);
            free(expected_child_dimensions);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        values = new_values;
        for (size_t i = 0; i < child_value_count; ++i) {
            values[count + i] = child_values[i];
        }
        // Missing cells in a shorter child are represented by null Values.
        for (size_t i = child_value_count; i < expected_child_total_count; ++i) {
            values[count + i] = value_create_null();
        }
        count += expected_child_total_count;
        block_count++;
        free(child_values);
        free(child_dimensions);

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (!use_outer_wrapper && token != NULL && token->type == TOKEN_LEFT_BRACE) {
            continue;
        }

        if (use_outer_wrapper && token != NULL && token->type == TOKEN_RIGHT_BRACE) {
            continue;
        }

        break;
    }

    if (use_outer_wrapper) {
        if (!parser_expect(parser, TOKEN_RIGHT_BRACE, "Syntax Error: Unterminated array initializer.")) {
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            free(expected_child_dimensions);
            return 0;
        }
    }

    if (declared_dimensions[0] != 0 && block_count != (size_t)declared_dimensions[0]) {
        fprintf(stderr, "Syntax Error: Expected %ld elements but found %zu.\n", declared_dimensions[0], block_count);
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        free(expected_child_dimensions);
        return 0;
    }

    out_dimensions[0] = (long)block_count;
    *out_values = values;
    *out_count = count;
    free(expected_child_dimensions);
    return 1;
}

/*
Parses the leaf values of a dynamic initializer until its closing brace. The
element type is inferred from the first value and checked for compatibility
with every later value.
*/
static int parse_dynamic_initializer_values(Parser *parser,
                                            ValueType *element_type,
                                            Value **out_values,
                                            size_t *out_count)
{

    size_t count = 0;
    Value *values = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_BRACE) {
            parser_advance(parser);
            break;
        }

        if (!is_literal_value_token(token)) {
            fprintf(stderr, "Syntax Error: Expected literal value in array initializer.\n");
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }

        Value value = parse_literal_value_token(token);
        if (count == 0) {
            *element_type = value.type;
        } else if (!array_update_element_type(element_type, value.type)) {
            value_free(&value);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            fprintf(stderr, "Type Error: Array elements must all have the same data type.\n");
            return 0;
        }

        parser_advance(parser);
        // Grow the flat value list only after the literal has passed type checks.
        Value *new_values = (Value *)realloc(values, (count + 1U) * sizeof(Value));
        if (new_values == NULL) {
            fprintf(stderr, "Error: out of memory while parsing array initializer\n");
            value_free(&value);
            for (size_t i = 0; i < count; ++i) {
                value_free(&values[i]);
            }
            free(values);
            return 0;
        }
        values = new_values;
        values[count++] = value;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token != NULL && token->type == TOKEN_RIGHT_BRACE) {
            continue;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or '}' in array initializer.\n");
        for (size_t i = 0; i < count; ++i) {
            value_free(&values[i]);
        }
        free(values);
        return 0;
    }

    *out_values = values;
    *out_count = count;
    return 1;
}

/*
Checks whether the next token begins an assignment initializer. This keeps the
declaration parser readable while distinguishing `var a[3]` from `var a[3] =`.
*/
static int array_declaration_has_initializer(Parser *parser)
{
    Token *token = parser_peek(parser);
    return token != NULL && token->type == TOKEN_EQUAL;
}

Value parse_array_declaration(Parser *parser, int *error)
{
    /*
    Builds an ArrayValue from the declaration suffix. The parser first reads
    dimensions, then chooses among inferred, fixed, and empty declarations;
    initializer values are transferred into the array's flat storage before
    temporary parser buffers are released.
    */
    long *dimensions = NULL;
    size_t dimension_count = 0;
    int dynamic_declaration = 0;
    Value result = value_create_string(NULL);
    if (!parse_array_dimensions(parser, &dimensions, &dimension_count, error, &dynamic_declaration)) {
        return result;
    }

    if (!array_declaration_has_initializer(parser) && dimension_count == 0) {
        fprintf(stderr, "Syntax Error: Expected '=' or array dimensions in variable declaration.\n");
        free(dimensions);
        *error = 1;
        return result;
    }

    int has_dynamic_dimension = 0;
    for (size_t i = 0; i < dimension_count; ++i) {
        if (dimensions[i] == 0) {
            has_dynamic_dimension = 1;
            break;
        }
    }

    if (array_declaration_has_initializer(parser)) {
        // An initializer determines values and, for dynamic dimensions, actual shape.
        parser_advance(parser);

        if (dimension_count == 0) {
            // No brackets means a one-dimensional dynamic array inferred entirely from its values.
            Value *values = NULL;
            size_t value_count = 0;
            ValueType element_type = VALUE_NULL;
            if (!parser_expect(parser, TOKEN_LEFT_BRACE, "Syntax Error: Expected '{' in array initializer.")) {
                free(dimensions);
                *error = 1;
                return result;
            }
            if (!parse_dynamic_initializer_values(parser, &element_type, &values, &value_count)) {
                free(dimensions);
                *error = 1;
                return result;
            }

            long *dynamic_dimensions = (long *)malloc(sizeof(long));
            if (dynamic_dimensions == NULL) {
                for (size_t i = 0; i < value_count; ++i) {
                    value_free(&values[i]);
                }
                free(values);
                free(dimensions);
                *error = 1;
                return result;
            }
            dynamic_dimensions[0] = (long)value_count;

            ArrayValue *array = array_value_create_from_values(1, dynamic_dimensions, element_type, values, value_count);
            array->is_dynamic = 1;
            for (size_t j = 0; j < value_count; ++j) {
                value_free(&values[j]);
            }
            free(values);
            free(dynamic_dimensions);
            free(dimensions);
            result = value_create_array(array);
            *error = 0;
            return result;
        }

        if (has_dynamic_dimension) {
            // At least one [] requires the recursive dynamic shape parser.
            long *actual_dimensions = (long *)malloc(dimension_count * sizeof(long));
            if (actual_dimensions == NULL) {
                free(dimensions);
                *error = 1;
                return result;
            }

            Value *values = NULL;
            size_t value_count = 0;
            ValueType element_type = VALUE_NULL;
            int initializer_parsed;
            if (dimension_count == 1) {
                initializer_parsed = parse_dynamic_initializer_level(parser, 0, dimension_count, dimensions,
                                                                      actual_dimensions, &element_type, &values,
                                                                      &value_count);
            } else {
                initializer_parsed = parse_dynamic_multi_dimensional_initializer(parser, dimension_count, dimensions,
                                                                                  actual_dimensions, &element_type, &values,
                                                                                  &value_count);
            }
            if (!initializer_parsed) {
                free(dimensions);
                free(actual_dimensions);
                *error = 1;
                return result;
            }

            ArrayValue *array = array_value_create_from_values(dimension_count, actual_dimensions, element_type, values, value_count);
            array->is_dynamic = 1;
            for (size_t j = 0; j < value_count; ++j) {
                value_free(&values[j]);
            }
            free(values);
            free(dimensions);
            free(actual_dimensions);
            result = value_create_array(array);
            *error = 0;
            return result;
        }

        Value *values = NULL;
        size_t value_count = 0;
        ValueType element_type = VALUE_NULL;
        if (!parse_multi_dimensional_initializer(parser, dimension_count, dimensions, &element_type, &values, &value_count)) {
            free(dimensions);
            *error = 1;
            return result;
        }

        size_t total_count = 1;
        // Fixed dimensions must describe exactly the number of parsed elements.
        for (size_t i = 0; i < dimension_count; ++i) {
            if (dimensions[i] < 0) {
                fprintf(stderr, "Runtime Error: Array size cannot be negative.\n");
                for (size_t j = 0; j < value_count; ++j) {
                    value_free(&values[j]);
                }
                free(values);
                free(dimensions);
                *error = 1;
                return result;
            }
            total_count *= (size_t)dimensions[i];
        }

        if (value_count == 0) {
            ArrayValue *array = array_value_create_fixed(dimension_count, dimensions, VALUE_NULL);
            free(dimensions);
            free(values);
            result = value_create_array(array);
            *error = 0;
            return result;
        }

        if (value_count != total_count) {
            fprintf(stderr, "Syntax Error: Expected %zu elements but found %zu.\n", total_count, value_count);
            for (size_t j = 0; j < value_count; ++j) {
                value_free(&values[j]);
            }
            free(values);
            free(dimensions);
            *error = 1;
            return result;
        }

        ArrayValue *array = array_value_create_from_values(dimension_count, dimensions, element_type, values, value_count);
        for (size_t j = 0; j < value_count; ++j) {
            value_free(&values[j]);
        }
        free(values);
        free(dimensions);
        result = value_create_array(array);
        *error = 0;
        return result;
    }

    if (dynamic_declaration) {
        // A dynamic declaration without an initializer starts empty but retains its rank.
        ArrayValue *array = array_value_create_fixed(dimension_count, dimensions, VALUE_NULL);
        array->is_dynamic = 1;
        free(dimensions);
        result = value_create_array(array);
        *error = 0;
        return result;
    }

    for (size_t i = 0; i < dimension_count; ++i) {
        if (dimensions[i] < 0) {
            fprintf(stderr, "Runtime Error: Array size cannot be negative.\n");
            free(dimensions);
            *error = 1;
            return result;
        }
    }

    ArrayValue *array = array_value_create_fixed(dimension_count, dimensions, VALUE_NULL);
    free(dimensions);
    result = value_create_array(array);
    *error = 0;
    return result;
}

/*
Reads a comma-separated list of literal integer indices. The resulting buffer
is owned by the caller and is used by array access and select parsing.
*/
static int parse_integer_sequence(Parser *parser, long **out_indices, size_t *out_count, int *error)
{
    if (error != NULL) {
        *error = 0;
    }
    size_t count = 0;
    long *indices = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL || token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected integer index in array access.\n");
            free(indices);
            *error = 1;
            return 0;
        }

        long index = 0;
        if (!parse_integer_token(token, &index)) {
            fprintf(stderr, "Syntax Error: Invalid array index.\n");
            free(indices);
            *error = 1;
            return 0;
        }

        parser_advance(parser);
        // Append the validated index to the dynamically sized index list.
        long *new_indices = (long *)realloc(indices, (count + 1U) * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading array index\n");
            free(indices);
            *error = 1;
            return 0;
        }
        indices = new_indices;
        indices[count++] = index;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        break;
    }

    *out_indices = indices;
    *out_count = count;
    return 1;
}

/*
Parses an identifier followed by zero or more bracket selectors. Empty brackets
represent whole-array access, integer or named brackets represent element
access, and a following dot delegates multi-value selection to select_parser.
*/
int parse_array_access(Parser *parser, ArrayAccess *access)
{
    array_access_init(access);
    Token *token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) {
        return 0;
    }

    access->name = drift_duplicate_string(token->value);
    // Store the array name separately because the token buffer belongs to the lexer.
    parser_advance(parser);

    while (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_LEFT_BRACKET) {
        parser_advance(parser);
        Token *next = parser_peek(parser);
        if (next == NULL) {
            fprintf(stderr, "Syntax Error: Expected array access content.\n");
            return 0;
        }

        if (next->type == TOKEN_RIGHT_BRACKET) {
            // Empty [] selects the complete array rank position.
            parser_advance(parser);
            access->is_whole_array = 1;
            access->whole_array_dimension_count++;
            continue;
        }

        if (access->is_whole_array) {
            fprintf(stderr, "Syntax Error: Cannot mix empty and indexed array access.\n");
            return 0;
        }

        if (next->type == TOKEN_LEFT_PAREN) {
            fprintf(stderr, "Syntax Error: Use select() for multi-value array access.\n");
            return 0;
        }

        long index = 0;
        char *index_name = NULL;
        if (next->type == TOKEN_IDENTIFIER) {
            // Named indices are resolved later, when the runtime environment exists.
            index_name = drift_duplicate_string(next->value);
            if (index_name == NULL) {
                fprintf(stderr, "Error: out of memory while reading array index\n");
                return 0;
            }
        } else if (!parse_integer_token(next, &index)) {
            fprintf(stderr, "Syntax Error: Invalid array index.\n");
            return 0;
        }
        parser_advance(parser);

        if (!parser_expect(parser, TOKEN_RIGHT_BRACKET, "Syntax Error: Expected ']' after array index.")) {
            return 0;
        }

        long *new_indices = (long *)realloc(access->indices, (access->index_count + 1U) * sizeof(long));
        char **new_index_names = (char **)realloc(access->index_names, (access->index_count + 1U) * sizeof(char *));
        if (new_indices == NULL || new_index_names == NULL) {
            fprintf(stderr, "Error: out of memory while reading array indices\n");
            free(index_name);
            return 0;
        }
        access->indices = new_indices;
        access->index_names = new_index_names;
        access->indices[access->index_count++] = index;
        access->index_names[access->index_count - 1U] = index_name;
    }

    if (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_DOT) {
        // Dot syntax belongs to the select parser, which extends this access object.
        return parse_select_access(parser, access);
    }

    return 1;
}

void array_access_init(ArrayAccess *access)
{
    // Initialize every pointer, count, and mode flag so cleanup is always safe.
    if (access == NULL) {
        return;
    }

    access->name = NULL;
    access->is_whole_array = 0;
    access->whole_array_dimension_count = 0;
    access->is_selection = 0;
    access->index_count = 0;
    access->indices = NULL;
    access->index_names = NULL;
    access->selection_count = 0;
    access->selection_tuple_size = 0;
    access->selection_indices = NULL;
    access->selection_breaks = NULL;
}

void array_access_free(ArrayAccess *access)
{
    /* Release all buffers owned by an access expression, including each named
       index string, then clear pointers to prevent accidental reuse. */
    if (access == NULL) {
        return;
    }

    free(access->name);
    free(access->indices);
    for (size_t i = 0; i < access->index_count; ++i) {
        free(access->index_names[i]);
    }
    free(access->index_names);
    free(access->selection_indices);
    free(access->selection_breaks);
    access->name = NULL;
    access->indices = NULL;
    access->index_names = NULL;
    access->selection_indices = NULL;
    access->selection_breaks = NULL;
}

Token *parser_peek(Parser *parser)
{
    // Look ahead without consuming a token; NULL marks the end of the token array.
    if (parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

Token *parser_advance(Parser *parser)
{
    // Return the current token and move the parser cursor forward by one.
    if (parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}
