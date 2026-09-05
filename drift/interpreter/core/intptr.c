/* SPDX-License-Identifier: AGPL-3.0-only */
/* Integer-pointer helpers support recursive value and container operations. 
Anything that needs to traverse values, arrays, or other containers can use these functions to
avoid duplicating the logic for converting between flat indices and multi-dimensional coordinates.
The functions provide a consistent way to handle integer pointers, which are used to represent
multi-dimensional indices in arrays. The functions include utilities for calculating the total number 
of elements in an array, converting between flat indices and multi-dimensional coordinates, and
validating indices against array dimensions. By using these helpers, the code can maintain a clear
and consistent approach to working with integer pointers, reducing the risk of errors and improving
code maintainability. The functions are designed to be efficient and handle edge cases, 
such as negative indices or indices that exceed the array's dimensions. They also provide error
reporting mechanisms to indicate when an operation fails due to invalid indices or other issues.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/array_value.h"
#include "drift/input.h"
#include "drift/function.h"
#include "drift/interpreter.h"
#include "drift/lexer.h"
#include "drift/operator_ternary.h"
#include "drift/parser.h"
#include "drift/repeat.h"

/*
We take a structure approach to managing integer pointers, which represent multi-dimensional indices 
in arrays. The structure encapsulates the necessary information for traversing and manipulating
multi-dimensional arrays, including the current index, the total number of dimensions, and the
size of each dimension. By using a structure, we can easily pass around the integer pointer state
and maintain consistency across different functions that operate on arrays. The structure also allows
for additional metadata to be added in the future, such as error flags or context information, without
changing the function signatures. This approach improves code readability and maintainability, as it
provides a clear representation of the integer pointer state and its associated data. It also enables 
the implementation of helper functions that can operate on the structure, such as incrementing the
current index, validating indices against dimensions, or converting between flat and multi-dimensional
indices. Overall, using a structure for integer pointers enhances the clarity and robustness of the
codebase, making it easier to work with multi-dimensional arrays and reducing the likelihood of errors.
*/
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;


// Initializes a StringBuilder with a default capacity. The function allocates memory for the
// StringBuilder's data buffer and sets the initial length to 0. If memory allocation fails
// during initialization, the function returns 0 to indicate failure. Otherwise, it returns 1
static int sb_init(StringBuilder *sb)
{
    if (sb == NULL) {
        return 0;
    }

    sb->capacity = 64;
    sb->length = 0;
    sb->data = (char *)malloc(sb->capacity);
    if (sb->data == NULL) {
        return 0;
    }
    sb->data[0] = '\0';
    return 1;
}


// Simply frees the allocated memory for the StringBuilder's data buffer and resets its fields to
// a safe state. This ensures that the StringBuilder can be reused or safely discarded without
// leaving dangling pointers or invalid state. The function checks if the StringBuilder pointer
// is NULL before attempting to free its data, preventing potential segmentation faults. After
// freeing the data, it sets the data pointer to NULL and resets the length and capacity to 0.
static void sb_free(StringBuilder *sb)
{
    if (sb == NULL) {
        return;
    }
    free(sb->data);
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

// Appends a string to the StringBuilder, resizing the buffer if necessary. 
static int sb_append(StringBuilder *sb, const char *text)
{
    if (sb == NULL || text == NULL) {
        return 0;
    }

    size_t text_length = strlen(text); // Calculate the length of the text to append
    size_t required = sb->length + text_length + 1;
    if (required > sb->capacity) {
        size_t new_capacity = sb->capacity * 2; // Double the capacity until it can accommodate the new text
        while (new_capacity < required) {
            new_capacity *= 2; // Keep doubling until we have enough capacity;
        }
        char *new_data = (char *)realloc(sb->data, new_capacity);
        if (new_data == NULL) {
            return 0;
        }
        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    // memcpy is used to copy the new text into the StringBuilder's data buffer, 
    // and the length is updated accordingly. The null terminator is added to ensure the string 
    // is properly terminated. This allows for safe string operations and prevents buffer overflows.
    memcpy(sb->data + sb->length, text, text_length);
    sb->length += text_length;
    sb->data[sb->length] = '\0';
    return 1;
}

// Appends one character while preserving the StringBuilder's null terminator.
static int sb_append_char(StringBuilder *sb, char c)
{
    if (sb == NULL) {
        return 0;
/*
Removes whitespace from expression fragments in place. This is used for repeat
step shorthand, where spaces should not change whether a counter form matches.
*/
    }

    if (sb->length + 2 > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        char *new_data = (char *)realloc(sb->data, new_capacity);
        if (new_data == NULL) {
            return 0;
        }
        sb->data = new_data;
        sb->capacity = new_capacity;
    }

    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
    return 1;
}

// The function appends a single character to the StringBuilder, resizing the buffer if necessary.
static int format_value_to_string(const Value *value, StringBuilder *sb);
// The function formats a Value object into a string representation, handling different types of values
static int format_array_value_to_string(const ArrayValue *array, StringBuilder *sb);
// The function formats an array value into a string representation.
static int format_array_recursive_to_string(const ArrayValue *array, size_t dimension, size_t base_offset, StringBuilder *sb);
// The function recursively formats a multi-dimensional array into a string representation, handling each dimension and its elements appropriately.
static int format_array_element_to_string(const ArrayValue *array, const long *indices, size_t index_count, StringBuilder *sb);
// The function formats a specific element of an array into a string representation based on the provided indices.
/*
Copies literal array indices and resolves indices written as variable names.
The returned array belongs to the caller and must be released with free().
*/
static int resolve_array_indices(const ArrayAccess *access, Environment *environment, long **out_indices)
{
    if (access == NULL || environment == NULL || out_indices == NULL) {
        return 0;
    }

    long *indices = NULL;
    if (access->index_count > 0) {
        indices = (long *)malloc(access->index_count * sizeof(long));
        if (indices == NULL) {
            fprintf(stderr, "Error: out of memory while resolving array indices\n");
            return 0;
        }
    }

    for (size_t i = 0; i < access->index_count; ++i) {
        indices[i] = access->indices[i];
        if (access->index_names != NULL && access->index_names[i] != NULL) {
            Value index_value;
            if (!environment_get(environment, access->index_names[i], &index_value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", access->index_names[i]);
                free(indices);
                return 0;
            }
            if (index_value.type != VALUE_INTEGER) {
                fprintf(stderr, "Runtime Error: Array index variable '%s' must be an integer.\n", access->index_names[i]);
                value_free(&index_value);
                free(indices);
                return 0;
            }
            indices[i] = index_value.integer_value;
            value_free(&index_value);
        }
    }

    *out_indices = indices;
    return 1;
}

/*
Walks a chain of indices through arrays and strings. Multidimensional arrays
are resolved in one operation, while one-dimensional nested values are stepped
through so expressions such as matrix[0][1] remain supported.
*/
static int resolve_nested_array_access(const Value *base_value, const ArrayAccess *access, Environment *environment, Value *out_value)
{
    Value current = value_create_null();
    long *indices = NULL;

    if (base_value == NULL || access == NULL || out_value == NULL) {
        return 0;
    }

    if (!resolve_array_indices(access, environment, &indices)) {
        return 0;
    }

    current = value_copy(base_value);
    for (size_t i = 0; i < access->index_count; ++i) {
        long index = indices[i];

        if (current.type == VALUE_STRING) {
            if (index < 0 || (size_t)index >= strlen(current.string_value ? current.string_value : "")) {
                fprintf(stderr, "Runtime Error: String index out of bounds.\n");
                value_free(&current);
                free(indices);
                return 0;
            }
            char single[2];
            single[0] = (current.string_value ? current.string_value[index] : '\0');
            single[1] = '\0';
            value_free(&current);
            current = value_create_string(single);
            continue;
        }

        if (current.type != VALUE_ARRAY || current.array_value == NULL) {
            fprintf(stderr, "Runtime Error: Only arrays and strings can be indexed.\n");
            value_free(&current);
            free(indices);
            return 0;
        }

        if (current.array_value->dimension_count != 1) {
            const Value *element = array_value_get_element(current.array_value, indices + i, access->index_count - i, &(int){0});
            if (element == NULL) {
                fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                value_free(&current);
                free(indices);
                return 0;
            }
            value_free(&current);
            current = value_copy(element);
            break;
        }

        if (index < 0 || (size_t)index >= current.array_value->length) {
            fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            value_free(&current);
            free(indices);
            return 0;
        }

        Value next = value_copy(&current.array_value->elements[index]);
        value_free(&current);
        current = next;
    }

    free(indices);
    *out_value = current;
    return 1;
}

/*
Uses the normal lexer and parser to decide whether template text is an array
access. Requiring the parser to consume EOF prevents ordinary text from being
mistaken for a partial access expression.
*/
static int parse_array_template_expression(const char *expr, ArrayAccess *access)
{
    if (expr == NULL || access == NULL) {
        return 0;
    }

    array_access_init(access);
    Lexer lexer = lexer_create(expr);
    size_t token_count = 0;
    Token *tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        return 0;
    }

    Parser parser = parser_create(tokens, token_count);
    int parsed = parse_array_access(&parser, access);
    int is_access = access->is_whole_array || access->is_selection || access->index_count > 0;
    int consumed_all = parser.index < parser.count && parser.tokens[parser.index].type == TOKEN_EOF;
    token_free_array(tokens, token_count);

    if (!parsed || !is_access || !consumed_all) {
        array_access_free(access);
        return 0;
    }

    return 1;
}

/*
Converts each runtime value type into the truth value used by if conditions
and logical operators. Empty strings and arrays, null, and numeric zero are
false; infinity is true.
*/
static int value_is_truthy(const Value *value)
{
    if (value == NULL) {
        return 0;
    }

    if (value->type == VALUE_BOOLEAN) {
        return value->boolean_value;
    }
    if (value->type == VALUE_INTEGER) {
        return value->integer_value != 0;
    }
    if (value->type == VALUE_FLOAT) {
        return value->float_value != 0.0;
    }
    if (value->type == VALUE_STRING) {
        return value->string_value != NULL && value->string_value[0] != '\0';
    }
    if (value->type == VALUE_NULL) {
        return 0;
    }
    if (value->type == VALUE_INFINITY) {
        return 1;
    }
    if (value->type == VALUE_ARRAY) {
        return value->array_value != NULL && value->array_value->length > 0;
    }
    return 0;
}

/*
Resolves one index against a one-dimensional array or string and returns a
fresh Value. Multidimensional access is handled by the expression evaluator's
array-access chain instead.
*/
static int resolve_indexed_value(const Value *container, long index, Value *out_value)
{
    if (container == NULL || out_value == NULL) {
        return 0;
    }

    *out_value = value_create_null();

    if (container->type == VALUE_ARRAY) {
        if (container->array_value == NULL || container->array_value->dimension_count != 1) {
            fprintf(stderr, "Runtime Error: Only 1D array elements can be indexed with a single integer.");
            return 0;
        }
        if (index < 0 || (size_t)index >= container->array_value->length) {
            fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
            return 0;
        }
        *out_value = value_copy(&container->array_value->elements[index]);
        return 1;
    }

    if (container->type == VALUE_STRING) {
        char *text = container->string_value ? container->string_value : "";
        size_t length = strlen(text);
        if (index < 0 || (size_t)index >= length) {
            fprintf(stderr, "Runtime Error: String index out of bounds.\n");
            return 0;
        }

        char single[2];
        single[0] = text[index];
        single[1] = '\0';
        *out_value = value_create_string(single);
        return 1;
    }

    fprintf(stderr, "Runtime Error: Only array and string values can be indexed.\n");
    return 0;
}

/*
Turns a lexer token into a runtime value. Identifiers are looked up in the
environment, while literals are parsed without mutating the token stream.
The ok flag distinguishes an unsupported token or missing variable from a
valid null value.
*/
static Value resolve_identifier_or_literal(Environment *environment, Token *token, int *ok)
{
    Value value = value_create_null();
    if (token == NULL || ok == NULL) {
/*
Returns the binding power used by the precedence-climbing expression parser.
Higher values bind more tightly; the ternary operator is intentionally the
loosest operator in the language.
*/
        *ok = 0;
        return value;
    }

    *ok = 1;
    if (token->type == TOKEN_INTEGER) {
        char *end = NULL;
        long v = strtol(token->value, &end, 10);
        (void)end;
        return value_create_integer(v);
    }
    if (token->type == TOKEN_FLOAT) {
        char *end = NULL;
        double v = strtod(token->value, &end);
        (void)end;
        return value_create_float(v);
    }
    if (token->type == TOKEN_STRING) {
        if (token->value != NULL && (token->value[0] == '"' || token->value[0] == '\'')) {
            size_t len = strlen(token->value);
            if (len >= 2 && token->value[len - 1] == token->value[0]) {
                char *unquoted = (char *)malloc(len - 1);
                if (unquoted != NULL) {
                    memcpy(unquoted, token->value + 1, len - 2);
                    unquoted[len - 2] = '\0';
                    Value v = value_create_string(unquoted);
                    free(unquoted);
                    return v;
                }
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
    if (token->type == TOKEN_IDENTIFIER) {
        if (!environment_get(environment, token->value, &value)) {
            fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", token->value);
            *ok = 0;
            return value_create_null();
        }
        return value;
    }

    *ok = 0;
    return value_create_null();
}

static int token_precedence(TokenType type)
{
    switch (type) {
        case TOKEN_QUESTION:
            return 1;
        case TOKEN_RANGE:
            return 2;
        case TOKEN_OR_OR:
            return 3;
        case TOKEN_AND_AND:
            return 4;
        case TOKEN_PIPE:
            return 5;
        case TOKEN_CARET:
            return 6;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
            return 7;
        case TOKEN_GREATER:
        case TOKEN_LESS:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_LESS_EQUAL:
            return 8;
        case TOKEN_SHIFT_LEFT:
        case TOKEN_SHIFT_RIGHT:
            return 9;
        case TOKEN_AMPERSAND:
            return 10;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 11;
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
            return 12;
        case TOKEN_IN:
            return 13;
        case TOKEN_IS:
            return 7;
        default:
            return -1;
    }
}

/*
Applies the unary operators supported by Drift. Unary operators return a new
Value, leaving the operand owned by the caller so it can be released normally.
*/
static Value apply_unary_operator(TokenType type, const Value *value)
{
    Value result = value_create_null();
    if (value == NULL) {
        return result;
    }

    if (type == TOKEN_MINUS) {
        if (value->type == VALUE_INTEGER) {
            return value_create_integer(-value->integer_value);
        }
        if (value->type == VALUE_FLOAT) {
            return value_create_float(-value->float_value);
        }
        return value_create_null();
    }
    if (type == TOKEN_BANG) {
        return value_create_boolean(!value_is_truthy(value));
    }
    if (type == TOKEN_TILDA) {
        if (value->type == VALUE_INTEGER) {
            return value_create_integer(~value->integer_value);
        }
        return value_create_null();
    }
    if (type == TOKEN_PLUS) {
        return value_copy(value);
    }
    return result;
}

/*
Evaluates one precedence-climbing expression directly from lexer tokens.
Besides binary operators, this routine handles parentheses, unary operators,
array or string indexing, and the right-hand branches of the ternary operator.
*/
static Value evaluate_expression_tokens(Environment *environment, Token *tokens, size_t count, size_t *index, int min_precedence, int *ok)
{
    Value left = value_create_null();
    const char *postfix_name = NULL;
    int can_postfix = 0;
    TokenType unary_type = TOKEN_UNKNOWN;
    Token *token = NULL;

    if (tokens == NULL || index == NULL || ok == NULL) {
        *ok = 0;
        return left;
    }

    if (*index >= count) {
        *ok = 0;
        return left;
    }

    token = &tokens[*index];
    if (token->type == TOKEN_PLUS || token->type == TOKEN_MINUS || token->type == TOKEN_BANG || token->type == TOKEN_TILDA) {
        unary_type = token->type;
        (*index)++;
    }

    if (*index >= count) {
        *ok = 0;
        return left;
    }

    token = &tokens[*index];
    if (token->type == TOKEN_LEFT_PAREN) {
        (*index)++;
        left = evaluate_expression_tokens(environment, tokens, count, index, 0, ok);
        if (!*ok || (*index >= count) || tokens[*index].type != TOKEN_RIGHT_PAREN) {
            *ok = 0;
            return left;
        }
        (*index)++;
    } else if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_INTEGER || token->type == TOKEN_FLOAT ||
               token->type == TOKEN_STRING || token->type == TOKEN_TRUE || token->type == TOKEN_FALSE ||
               token->type == TOKEN_NULL || token->type == TOKEN_INFINITY) {
        if (token->type == TOKEN_IDENTIFIER) {
            // Keep the name so a following ++ or -- can update its environment binding.
            postfix_name = token->value;
            can_postfix = 1;
        }
        left = resolve_identifier_or_literal(environment, token, ok);
        (*index)++;
        while (*index < count && tokens[*index].type == TOKEN_LEFT_BRACKET) {
            // Indexed values are not direct environment bindings, so postfix updates apply only to plain identifiers.
            can_postfix = 0;
            Value index_value;
            long arr_index = 0;
            (*index)++;
            index_value = evaluate_expression_tokens(environment, tokens, count, index, 0, ok);
            if (!*ok) {
                return left;
            }
            if (index_value.type != VALUE_INTEGER) {
                fprintf(stderr, "Runtime Error: Array index must be an integer.\n");
                *ok = 0;
                value_free(&index_value);
                return left;
            }
            arr_index = index_value.integer_value;
            value_free(&index_value);
            if (*index >= count || tokens[*index].type != TOKEN_RIGHT_BRACKET) {
                fprintf(stderr, "Runtime Error: Expected ']' after array index.\n");
                *ok = 0;
                return left;
            }
            (*index)++;

            Value next_value = value_create_null();
            if (!resolve_indexed_value(&left, arr_index, &next_value)) {
                *ok = 0;
                return left;
            }
            left = next_value;
        }
    } else {
        *ok = 0;
        return left;
    }

    if (unary_type != TOKEN_UNKNOWN) {
        left = apply_unary_operator(unary_type, &left);
    }

    if (can_postfix && *index < count &&
        (tokens[*index].type == TOKEN_PLUS_PLUS || tokens[*index].type == TOKEN_MINUS_MINUS)) {
        TokenType postfix_type = tokens[*index].type;
        Value replacement = operator_apply(postfix_type == TOKEN_PLUS_PLUS ? OPERATOR_INCREMENT : OPERATOR_DECREMENT,
                                            &left, NULL);
        if (replacement.type == VALUE_NULL || !environment_set(environment, postfix_name, &replacement)) {
            value_free(&replacement);
            fprintf(stderr, "Runtime Error: Increment/decrement requires a valid variable value.\n");
            *ok = 0;
            return left;
        }
        value_free(&replacement);
        (*index)++;
    }

    while (*index < count) {
        TokenType op = tokens[*index].type;
        if (op == TOKEN_COLON) {
            break;
        }
        if (op == TOKEN_TILDA) {
            fprintf(stderr, "Runtime Error: Bitwise NOT is unary and must be written as '~value'.\n");
            *ok = 0;
            return left;
        }

        int precedence = token_precedence(op);
        if (precedence < min_precedence) {
            break;
        }

        (*index)++;

        if (op == TOKEN_QUESTION) {
            Value true_value = evaluate_expression_tokens(environment, tokens, count, index, 0, ok);
            Value false_value;

            if (!*ok || *index >= count || tokens[*index].type != TOKEN_COLON) {
                value_free(&true_value);
                *ok = 0;
                return left;
            }

            (*index)++;
            false_value = evaluate_expression_tokens(environment, tokens, count, index, precedence, ok);
            if (!*ok) {
                value_free(&true_value);
                return left;
            }

            {
                Value selected = operator_apply_ternary(&left, &true_value, &false_value);
                value_free(&left);
                value_free(&true_value);
                value_free(&false_value);
                left = selected;
            }
            continue;
        }

        if (*index >= count) {
            *ok = 0;
            return left;
        }

        Value right = evaluate_expression_tokens(environment, tokens, count, index, precedence + 1, ok);
        if (!*ok) {
            return left;
        }

        if (op == TOKEN_IN) {
            left = operator_apply(OPERATOR_IN, &left, &right);
        } else if (op == TOKEN_IS) {
            left = operator_apply(OPERATOR_IS, &left, &right);
        } else if (op == TOKEN_RANGE) {
            left = operator_apply(OPERATOR_RANGE, &left, &right);
        } else if (op == TOKEN_PLUS) {
            left = operator_apply(OPERATOR_ADD, &left, &right);
        } else if (op == TOKEN_MINUS) {
            left = operator_apply(OPERATOR_SUBTRACT, &left, &right);
        } else if (op == TOKEN_STAR) {
            left = operator_apply(OPERATOR_MULTIPLY, &left, &right);
        } else if (op == TOKEN_SLASH) {
            left = operator_apply(OPERATOR_DIVIDE, &left, &right);
        } else if (op == TOKEN_PERCENT) {
            left = operator_apply(OPERATOR_MODULO, &left, &right);
        } else if (op == TOKEN_EQUAL_EQUAL) {
            left = operator_apply(OPERATOR_EQUAL_EQUAL, &left, &right);
        } else if (op == TOKEN_NOT_EQUAL) {
            left = operator_apply(OPERATOR_NOT_EQUAL, &left, &right);
        } else if (op == TOKEN_GREATER) {
            left = operator_apply(OPERATOR_GREATER, &left, &right);
        } else if (op == TOKEN_LESS) {
            left = operator_apply(OPERATOR_LESS, &left, &right);
        } else if (op == TOKEN_GREATER_EQUAL) {
            left = operator_apply(OPERATOR_GREATER_EQUAL, &left, &right);
        } else if (op == TOKEN_LESS_EQUAL) {
            left = operator_apply(OPERATOR_LESS_EQUAL, &left, &right);
        } else if (op == TOKEN_AND_AND) {
            left = operator_apply(OPERATOR_AND_AND, &left, &right);
        } else if (op == TOKEN_OR_OR) {
            left = operator_apply(OPERATOR_OR_OR, &left, &right);
        } else if (op == TOKEN_AMPERSAND) {
            left = operator_apply(OPERATOR_BITWISE_AND, &left, &right);
        } else if (op == TOKEN_PIPE) {
            left = operator_apply(OPERATOR_BITWISE_OR, &left, &right);
        } else if (op == TOKEN_CARET) {
            left = operator_apply(OPERATOR_BITWISE_XOR, &left, &right);
        } else if (op == TOKEN_SHIFT_LEFT) {
            left = operator_apply(OPERATOR_SHIFT_LEFT, &left, &right);
        } else if (op == TOKEN_SHIFT_RIGHT) {
            left = operator_apply(OPERATOR_SHIFT_RIGHT, &left, &right);
        } else if (op == TOKEN_TILDA) {
            fprintf(stderr, "Runtime Error: Bitwise NOT is unary and must be written as '~value'.\n");
            *ok = 0;
            value_free(&right);
            return left;
        } else {
            *ok = 0;
            return left;
        }

        value_free(&right);
    }

    return left;
}

/*
Creates a temporary lexer/parser input for an expression stored in the AST,
evaluates it, and rejects any non-EOF tokens left after the expression.
*/
static Value evaluate_expression_text(Environment *environment, const char *expression, int *ok)
{
    Lexer lexer;
    Token *tokens = NULL;
    size_t token_count = 0;
    Value result = value_create_null();
    size_t index = 0;

    if (expression == NULL || ok == NULL) {
        if (ok != NULL) {
            *ok = 0;
        }
        return result;
    }

    lexer = lexer_create(expression);
    tokens = lexer_scan_all(&lexer, &token_count);
    if (tokens == NULL) {
        *ok = 0;
        return result;
    }

    *ok = 1;
    result = evaluate_expression_tokens(environment, tokens, token_count, &index, 0, ok);
    if (*ok && index < token_count && tokens[index].type != TOKEN_EOF) {
        *ok = 0;
    }

    token_free_array(tokens, token_count);
    return result;
}

Value interpreter_evaluate_expression(Environment *environment, const char *expression, int *ok)
{
    return evaluate_expression_text(environment, expression, ok);
}

/*
Writes a runtime value using Drift's user-facing representation. Arrays use the
array module's printer because their layout depends on their dimensions.
*/
static void print_value(const Value *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            printf("%ld", value->integer_value);
            break;
        case VALUE_FLOAT:
            printf("%g", value->float_value);
            break;
        case VALUE_STRING:
            printf("%s", value->string_value ? value->string_value : "");
            break;
        case VALUE_BOOLEAN:
            printf(value->boolean_value ? "true" : "false");
            break;
        case VALUE_NULL:
            printf("null");
            break;
        case VALUE_INFINITY:
            printf("infinity");
            break;
        case VALUE_ARRAY:
            print_array_value(value->array_value);
            break;
        default:
            break;
    }
}

/*
Appends a scalar or array value to a StringBuilder. This is the formatting path
used by interpolation, where output must be assembled before it is printed.
*/
static int format_value_to_string(const Value *value, StringBuilder *sb)
{
    char buffer[128];

    if (value == NULL || sb == NULL) {
        return 0;
    }

    switch (value->type) {
        case VALUE_INTEGER:
            snprintf(buffer, sizeof(buffer), "%ld", value->integer_value);
            return sb_append(sb, buffer);
        case VALUE_FLOAT:
            snprintf(buffer, sizeof(buffer), "%g", value->float_value);
            return sb_append(sb, buffer);
        case VALUE_STRING:
            return sb_append(sb, value->string_value ? value->string_value : "");
        case VALUE_BOOLEAN:
            return sb_append(sb, value->boolean_value ? "true" : "false");
        case VALUE_NULL:
            return sb_append(sb, "null");
        case VALUE_INFINITY:
            return sb_append(sb, "infinity");
        case VALUE_ARRAY:
            return format_array_value_to_string(value->array_value, sb);
        default:
            return 0;
    }
}

/*
Recursively formats an array in row-major order. The base offset identifies the
first element of the current slice, and stride skips the dimensions below it.
*/
static int format_array_recursive_to_string(const ArrayValue *array, size_t dimension, size_t base_offset, StringBuilder *sb)
{
    if (array == NULL || sb == NULL) {
        return 0;
    }

    if (dimension + 1 == array->dimension_count) {
        for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
            if (i > 0) {
                if (!sb_append_char(sb, ',')) {
                    return 0;
                }
            }
            if (!format_value_to_string(&array->elements[base_offset + i], sb)) {
                return 0;
            }
        }
        return 1;
    }

    size_t stride = 1;
    for (size_t i = dimension + 1; i < array->dimension_count; ++i) {
        stride *= (size_t)array->dimensions[i];
    }

    for (size_t i = 0; i < (size_t)array->dimensions[dimension]; ++i) {
        if(i > 0) {
            if (!sb_append_char(sb, '\n')) {
                return 0;
            }
            if(dimension == 0 && array->dimension_count > 2) {
                if (!sb_append_char(sb, '\n')) {
                    return 0;
                }
            }
        }
        if (!format_array_recursive_to_string(array, dimension + 1, base_offset + i * stride, sb)) {
            return 0;
        }
    }

    return 1;
}

/*
Formats a complete array, including the special case of an empty dynamic array.
*/
static int format_array_value_to_string(const ArrayValue *array, StringBuilder *sb)
{
    if (array == NULL || sb == NULL) {
        return 0;
    }

    if (array->is_dynamic && array->length == 0) {
        return 1;
    }

    return format_array_recursive_to_string(array, 0, 0, sb);
}

/*
Looks up one multidimensional array element and appends its formatted value.
The array module owns bounds checking; this helper translates failure into the
interpreter's runtime error message.
*/
static int format_array_element_to_string(const ArrayValue *array, const long *indices, size_t index_count, StringBuilder *sb)
{
    int error = 0;
    const Value *value = array_value_get_element(array, indices, index_count, &error);
    if (value == NULL) {
        if (error) {
            fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
        }
        return 0;
    }

    return format_value_to_string(value, sb);
}

/*
Expands {name} and explicit array references inside a print or input prompt.
Array references are parsed and formatted separately so whole arrays, selected
elements, and scalar values retain their language-specific rules.
*/
static char *interpolate_template(const char *template, Environment *environment)
{
    char *result;
    size_t result_capacity = 64;
    size_t result_length = 0;
    size_t i = 0;

    if (template == NULL) {
        return NULL;
    }

    result = (char *)malloc(result_capacity);
    if (result == NULL) {
        fprintf(stderr, "Error: out of memory while interpolating template\n");
        return NULL;
    }
    result[0] = '\0';

    while (template[i] != '\0') {
        if (template[i] == '{' && template[i + 1] != '\0') {
            size_t start = i + 1;
            size_t end = start;
            char *name;
            Value value;
            char *formatted = NULL;
            size_t formatted_length = 0;

            while (template[end] != '\0' && template[end] != '}') {
                end++;
            }

            if (template[end] == '\0') {
                free(result);
                return NULL;
            }

            name = (char *)malloc(end - start + 1U);
            if (name == NULL) {
                free(result);
                return NULL;
            }
            memcpy(name, template + start, end - start);
            name[end - start] = '\0';

            ArrayAccess array_access;
            int has_array_template = parse_array_template_expression(name, &array_access);
            if (has_array_template) {
                Value array_value;
                if (!environment_get(environment, array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", array_access.name);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                if (array_value.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", array_access.name);
                    value_free(&array_value);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                StringBuilder sb;
                if (!sb_init(&sb)) {
                    fprintf(stderr, "Error: out of memory while interpolating template\n");
                    value_free(&array_value);
                    array_access_free(&array_access);
                    free(name);
                    free(result);
                    return NULL;
                }

                int success;
                if (array_access.is_whole_array) {
                    if (array_access.whole_array_dimension_count != array_value.array_value->dimension_count) {
                        fprintf(stderr, "Runtime Error: Array rank is %zu; use %zu empty array access selector%s.\n",
                                array_value.array_value->dimension_count, array_value.array_value->dimension_count,
                                array_value.array_value->dimension_count == 1 ? "" : "s");
                        success = 0;
                    } else {
                        success = format_array_value_to_string(array_value.array_value, &sb);
                    }
                } else if (array_access.is_selection) {
                    success = array_access.selection_tuple_size == array_value.array_value->dimension_count;
                    if (!success) {
                        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %zu.\n",
                                array_access.selection_tuple_size, array_value.array_value->dimension_count);
                    }
                    for (size_t i = 0; success && i < array_access.selection_count; ++i) {
                        if (i > 0 && !sb_append_char(&sb, ' ')) {
                            success = 0;
                            break;
                        }
                        success = format_array_element_to_string(array_value.array_value,
                                                                 array_access.selection_indices + i * array_access.selection_tuple_size,
                                                                 array_access.selection_tuple_size, &sb);
                    }
                } else {
                    success = format_array_element_to_string(array_value.array_value, array_access.indices, array_access.index_count, &sb);
                }

                value_free(&array_value);
                array_access_free(&array_access);
                if (!success) {
                    sb_free(&sb);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted = sb.data;
            } else {
                if (!environment_get(environment, name, &value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", name);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted = (char *)malloc(128);
                if (formatted == NULL) {
                    value_free(&value);
                    free(name);
                    free(result);
                    return NULL;
                }

                formatted[0] = '\0';
                if (value.type == VALUE_INTEGER) {
                    snprintf(formatted, 128, "%ld", value.integer_value);
                } else if (value.type == VALUE_FLOAT) {
                    snprintf(formatted, 128, "%g", value.float_value);
                } else if (value.type == VALUE_STRING) {
                    snprintf(formatted, 128, "%s", value.string_value ? value.string_value : "");
                } else if (value.type == VALUE_BOOLEAN) {
                    snprintf(formatted, 128, "%s", value.boolean_value ? "true" : "false");
                } else if (value.type == VALUE_NULL) {
                    snprintf(formatted, 128, "null");
                } else if (value.type == VALUE_INFINITY) {
                    snprintf(formatted, 128, "infinity");
                } else if (value.type == VALUE_ARRAY) {
                    fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", name);
                    value_free(&value);
                    free(name);
                    free(formatted);
                    free(result);
                    return NULL;
                }
                value_free(&value);
            }

            formatted_length = strlen(formatted);
            while (result_length + formatted_length + 1 > result_capacity) {
                result_capacity *= 2;
                result = (char *)realloc(result, result_capacity);
                if (result == NULL) {
                    fprintf(stderr, "Error: out of memory while interpolating template\n");
                    free(name);
                    free(formatted);
                    return NULL;
                }
            }

            strcat(result, formatted);
            result_length += formatted_length;
            i = end + 1;
            free(name);
            free(formatted);
            continue;
        }

        while (result_length + 2 > result_capacity) {
            result_capacity *= 2;
            result = (char *)realloc(result, result_capacity);
            if (result == NULL) {
                fprintf(stderr, "Error: out of memory while interpolating template\n");
                return NULL;
            }
        }

        result[result_length++] = template[i++];
        result[result_length] = '\0';
    }

    return result;
}

/*
Resolves an input target when the caller needs its current value. Missing input
targets are represented as an empty string so the input subsystem can populate
them consistently.
*/
static int resolve_input_target_name(const char *target_name, Environment *environment, Value *out_value)
{
    if (target_name == NULL || out_value == NULL) {
        return 0;
    }

    if (!environment_get(environment, target_name, out_value)) {
        *out_value = value_create_string("");
    }

    return 1;
}

/*
Executes one already-parsed statement. This is the interpreter's main dispatch
boundary: each branch evaluates runtime data, updates the environment, and
returns nonzero as soon as a child statement or runtime operation fails.
*/
int interpreter_execute(Statement statement, Environment *environment)
{
    if (environment == NULL) {
        fprintf(stderr, "Runtime Error: missing execution environment.\n");
        return 1;
    }

    if (statement.type == STATEMENT_BREAK) {
        return interpreter_execute_break();
    }

    if (statement.type == STATEMENT_CONTINUE) {
        return interpreter_execute_continue();
    }

    if (statement.type == STATEMENT_FUNCTION) {
        return interpreter_execute_function(&statement.as.function_statement, environment);
    }

    if (statement.type == STATEMENT_FUNCTION_CALL) {
        int result = interpreter_execute_function_call(&statement.as.function_call_statement, environment);
        if (result == DRIFT_EXECUTION_OK && environment->has_return_value) {
            value_free(&environment->return_value);
            environment->has_return_value = 0;
        }
        return result;
    }

    if (statement.type == STATEMENT_RETURN) {
        return interpreter_execute_return(&statement.as.return_statement, environment);
    }

    /* Input prompts are interpolated before the input subsystem stores values. */
    if (statement.type == STATEMENT_INPUT) {
        InputStatement *input_statement = &statement.as.input_statement;

        if (input_statement->items == NULL || input_statement->count == 0) {
            fprintf(stderr, "Runtime Error: Empty input statement.\n");
            return 1;
        }

        for (size_t i = 0; i < input_statement->count; ++i) {
            InputItem *item = &input_statement->items[i];
            Value value;
            char *resolved_prompt = NULL;
            char *target_name = item->has_target ? item->target_name : "__temp_input__";

            if (item->has_prompt) {
                resolved_prompt = interpolate_template(item->prompt, environment);
                if (resolved_prompt == NULL) {
                    return 1;
                }
            }

            if (!drift_prompt_and_store(resolved_prompt, target_name, &value)) {
                free(resolved_prompt);
                fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", target_name);
                return 1;
            }

            if (item->has_target) {
                if (!environment_set(environment, item->target_name, &value)) {
                    value_free(&value);
                    free(resolved_prompt);
                    fprintf(stderr, "Runtime Error: Unable to store input in variable '%s'.\n", item->target_name);
                    return 1;
                }
            }

            value_free(&value);
            free(resolved_prompt);
        }

        return 0;
    }

    /* Print supports expressions, scalar references, templates, and explicit array access. */
    if (statement.type == STATEMENT_PRINT) {
        PrintStatement *print_statement = &statement.as.print_statement;
        Value value;
        char *resolved = NULL;

        if (print_statement->has_expression) {
            int ok = 0;
            Value expr_value = evaluate_expression_text(environment, print_statement->expression_text, &ok);
            if (!ok) {
                fprintf(stderr, "Runtime Error: Failed to evaluate expression '%s'.\n", print_statement->expression_text ? print_statement->expression_text : "");
                return 1;
            }
            print_value(&expr_value);
            value_free(&expr_value);
            printf("\n");
            return 0;
        }

        if (print_statement->value == NULL && !print_statement->has_array_access) {
            return 1;
        }

        if (print_statement->has_array_access) {
            Value value;
            if (!environment_get(environment, print_statement->array_access.name, &value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", print_statement->array_access.name);
                return 1;
            }

            if (print_statement->value != NULL) {
                resolved = interpolate_template(print_statement->value, environment);
                if (resolved == NULL) {
                    value_free(&value);
                    return 1;
                }
                printf("%s", resolved);
                free(resolved);
            }

            if (value.type == VALUE_STRING) {
                long *indices = NULL;
                if (!resolve_array_indices(&print_statement->array_access, environment, &indices)) {
                    value_free(&value);
                    return 1;
                }
                if (print_statement->array_access.is_whole_array || print_statement->array_access.is_selection || print_statement->array_access.index_count != 1) {
                    fprintf(stderr, "Runtime Error: String values only support single-index access.\n");
                    free(indices);
                    value_free(&value);
                    return 1;
                }

                Value indexed = value_create_null();
                if (!resolve_indexed_value(&value, indices[0], &indexed)) {
                    free(indices);
                    value_free(&value);
                    return 1;
                }
                print_value(&indexed);
                value_free(&indexed);
                free(indices);
                value_free(&value);
                printf("\n");
                return 0;
            }

            if (value.type != VALUE_ARRAY) {
                fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", print_statement->array_access.name);
                value_free(&value);
                return 1;
            }

            if (print_statement->array_access.is_whole_array) {
                if (print_statement->array_access.whole_array_dimension_count != value.array_value->dimension_count) {
                    fprintf(stderr, "Runtime Error: Array rank is %zu; use %zu empty array access selector%s.\n",
                            value.array_value->dimension_count, value.array_value->dimension_count,
                            value.array_value->dimension_count == 1 ? "" : "s");
                    value_free(&value);
                    return 1;
                }
                print_array_value(value.array_value);
            } else if (print_statement->array_access.is_selection) {
                print_array_selection(value.array_value, &print_statement->array_access);
            } else if (print_statement->array_access.index_count > 1 && value.array_value != NULL && value.array_value->dimension_count == 1) {
                Value resolved_value = value_create_null();
                if (!resolve_nested_array_access(&value, &print_statement->array_access, environment, &resolved_value)) {
                    value_free(&value);
                    return 1;
                }
                print_value(&resolved_value);
                value_free(&resolved_value);
            } else {
                long *indices = NULL;
                if (!resolve_array_indices(&print_statement->array_access, environment, &indices)) {
                    value_free(&value);
                    return 1;
                }
                print_array_element(value.array_value, indices, print_statement->array_access.index_count);
                free(indices);
            }

            value_free(&value);
            printf("\n");
            return 0;
        }

        if (print_statement->is_variable_reference) {
            if (!environment_get(environment, print_statement->value, &value)) {
                fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", print_statement->value);
                return 1;
            }
            print_value(&value);
            value_free(&value);
        } else {
            resolved = interpolate_template(print_statement->value, environment);
            if (resolved == NULL) {
                return 1;
            }
            printf("%s", resolved);
            free(resolved);
        }

        printf("\n");
        return 0;
    }

    /* Conditions are evaluated in order; the first truthy branch owns execution. */
    if (statement.type == STATEMENT_IF) {
        IfStatement *if_statement = &statement.as.if_statement;
        char *condition_text = NULL;
        int condition_ok = 0;
        Value condition_value;

        if (if_statement == NULL || if_statement->branches == NULL || if_statement->branch_count == 0) {
            fprintf(stderr, "Runtime Error: Invalid if statement.\n");
            return 1;
        }

        // 
        for (size_t i = 0; i < if_statement->branch_count; ++i) {
            condition_text = if_statement->branches[i].condition_text;
            if (condition_text == NULL || condition_text[0] == '\0') {
                fprintf(stderr, "Runtime Error: Empty if condition.\n");
                return 1;
            }

            condition_value = evaluate_expression_text(environment, condition_text, &condition_ok);
            if (!condition_ok) {
                fprintf(stderr, "Runtime Error: Failed to evaluate condition '%s'.\n", condition_text);
                value_free(&condition_value);
                return 1;
            }

            if (value_is_truthy(&condition_value)) {
                for (size_t j = 0; j < if_statement->branches[i].body_count; ++j) {
                    int result = interpreter_execute(if_statement->branches[i].body[j], environment);
                    if (result != 0) {
                        value_free(&condition_value);
                        return result;
                    }
                }
                value_free(&condition_value);
                return 0;
            }

            value_free(&condition_value);
        }

        if (if_statement->else_body != NULL && if_statement->else_count > 0) {
            for (size_t j = 0; j < if_statement->else_count; ++j) {
                int result = interpreter_execute(if_statement->else_body[j], environment);
                if (result != 0) {
                    return result;
                }
            }
        }

        return 0;
    }

    /* Repeat execution is implemented in the dedicated loop module. */
    if (statement.type == STATEMENT_REPEAT) {
        return interpreter_execute_repeat(&statement.as.repeat_statement, environment);
    }

    if (statement.type == STATEMENT_FOR) {
        return interpreter_execute_for(&statement.as.for_statement, environment);
    }

    if (statement.type == STATEMENT_WHILE) {
        return interpreter_execute_while(&statement.as.while_statement, environment);
    }

    // NEw: each statement execution is handled in the dedicated each module. Continues to the 
    // next statement if the each statement is not executed.
    if (statement.type == STATEMENT_EACH) {
        return interpreter_execute_each(&statement.as.each_statement, environment);
    }

    if (statement.type == STATEMENT_UNLESS) {
        return interpreter_execute_unless(&statement.as.unless_statement, environment);
    }

    if (statement.type == STATEMENT_WHEN) {
        return interpreter_execute_when(&statement.as.when_statement, environment);
    }


    /* Declarations and assignments share this path so every stored value enters the environment uniformly. */
    if (statement.type == STATEMENT_VARIABLE_DECLARATION) {
        VariableDeclaration *declaration = &statement.as.variable_declaration;

        if (declaration->vars == NULL || declaration->count == 0) {
            return 1;
        }

        for (size_t i = 0; i < declaration->count; ++i) {
            VariableDeclarationSingle *single = &declaration->vars[i];
            Value value;

            if (single->name == NULL) {
                if (single->is_declaration) {
                    fprintf(stderr, "Syntax Error: Expected variable identifier after 'var'.\n");
                } else {
                    fprintf(stderr, "Syntax Error: Expected variable identifier.\n");
                }
                return 1;
            }

            /* Array element assignment mutates a copied array, then replaces the environment binding. */
            if (single->is_array_element_assignment) {
                Value array_value;
                long *indices = NULL;
                if (!environment_get(environment, single->array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }
                if (array_value.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&array_value);
                    return 1;
                }
                if (!resolve_array_indices(&single->array_access, environment, &indices) ||
                    !array_value_set_element(array_value.array_value, indices, single->array_access.index_count, &single->value)) {
                    fprintf(stderr, "Runtime Error: Invalid array assignment.\n");
                    free(indices);
                    value_free(&array_value);
                    return 1;
                }
                free(indices);
                if (!environment_set(environment, single->array_access.name, &array_value)) {
                    fprintf(stderr, "Runtime Error: Unable to update array '%s'.\n", single->array_access.name);
                    value_free(&array_value);
                    return 1;
                }
                value_free(&array_value);
                continue;
            }

            if (single->is_declaration && environment_exists(environment, single->name)) {
                fprintf(stderr, "Runtime Error: Variable '%s' is already declared.\n", single->name);
                return 1;
            }

            /* Input declarations either store into an explicit target or use the declared name. */
            if (single->is_input_expression) {
                Value prompt_value = value_create_null();
                char *resolved_prompt = NULL;

                if (single->input_prompt != NULL) {
                    resolved_prompt = interpolate_template(single->input_prompt, environment);
                    if (resolved_prompt == NULL) {
                        return 1;
                    }
                }

                if (single->input_target != NULL) {
                    if (!drift_prompt_and_store(resolved_prompt, single->input_target, &prompt_value)) {
                        free(resolved_prompt);
                        fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", single->input_target);
                        return 1;
                    }
                    value = prompt_value;
                } else {
                    if (!drift_prompt_and_store(resolved_prompt, single->name, &prompt_value)) {
                        free(resolved_prompt);
                        fprintf(stderr, "Runtime Error: Unable to read input for '%s'.\n", single->name);
                        return 1;
                    }
                    value = prompt_value;
                }

                free(resolved_prompt);
            /* Ordinary declarations evaluate their expression at execution time. */
            } else if (single->has_expression) {
                int expr_ok = 0;
                Value expr_value = evaluate_expression_text(environment, single->expression_text, &expr_ok);
                if (!expr_ok) {
                    fprintf(stderr, "Runtime Error: Failed to evaluate expression '%s'.\n", single->expression_text ? single->expression_text : "");
                    return 1;
                }
                value = expr_value;
            /* Compound assignments read the current value, apply the selected operator, and replace it. */
            } else if (single->is_assignment && single->has_assignment_operator) {
                Value current_value = value_create_null();
                if (!environment_get(environment, single->name, &current_value)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->name);
                    return 1;
                }

                if (single->assignment_operator == OPERATOR_INCREMENT || single->assignment_operator == OPERATOR_DECREMENT) {
                    value = operator_apply(single->assignment_operator, &current_value, NULL);
                    value_free(&current_value);
                } else {
                    int expr_ok = 0;
                    Value rhs = evaluate_expression_text(environment, single->expression_text, &expr_ok);
                    if (!expr_ok) {
                        value_free(&current_value);
                        fprintf(stderr, "Runtime Error: Failed to evaluate assignment expression '%s'.\n", single->expression_text ? single->expression_text : "");
                        return 1;
                    }

                    if (single->assignment_operator == OPERATOR_ADD_ASSIGN) {
                        value = operator_apply(OPERATOR_ADD, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_SUBTRACT_ASSIGN) {
                        value = operator_apply(OPERATOR_SUBTRACT, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_MULTIPLY_ASSIGN) {
                        value = operator_apply(OPERATOR_MULTIPLY, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_DIVIDE_ASSIGN) {
                        value = operator_apply(OPERATOR_DIVIDE, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_MODULO_ASSIGN) {
                        value = operator_apply(OPERATOR_MODULO, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_AND_ASSIGN) {
                        value = operator_apply(OPERATOR_BITWISE_AND, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_OR_ASSIGN) {
                        value = operator_apply(OPERATOR_BITWISE_OR, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_XOR_ASSIGN) {
                        value = operator_apply(OPERATOR_BITWISE_XOR, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_SHIFT_LEFT_ASSIGN) {
                        value = operator_apply(OPERATOR_SHIFT_LEFT, &current_value, &rhs);
                    } else if (single->assignment_operator == OPERATOR_SHIFT_RIGHT_ASSIGN) {
                        value = operator_apply(OPERATOR_SHIFT_RIGHT, &current_value, &rhs);
                    } else {
                        value = rhs;
                    }

                    value_free(&current_value);
                    value_free(&rhs);
                }
            /* A multi-value select becomes a dynamic array; a single selection remains scalar. */
            } else if (single->is_assignment && single->is_array_expression) {
                Value source;
                if (!environment_get(environment, single->array_access.name, &source)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }

                if (source.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                }

                if (!single->array_access.is_selection) {
                    fprintf(stderr, "Syntax Error: Only select() expressions are supported for assignment.\n");
                    value_free(&source);
                    return 1;
                }

                Value *values = (Value *)malloc(single->array_access.selection_count * sizeof(Value));
                if (values == NULL) {
                    fprintf(stderr, "Error: out of memory while building dynamic array\n");
                    value_free(&source);
                    return 1;
                }

                if (single->array_access.selection_tuple_size != source.array_value->dimension_count) {
                    fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", single->array_access.selection_tuple_size, source.array_value->dimension_count);
                    free(values);
                    value_free(&source);
                    return 1;
                }

                if (single->array_access.selection_count == 1) {
                    const long *indices = single->array_access.selection_indices;
                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, indices, single->array_access.selection_tuple_size, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        free(values);
                        value_free(&source);
                        return 1;
                    }
                    value = value_copy(element);
                    free(values);
                    value_free(&source);
                } else {
                    for (size_t k = 0; k < single->array_access.selection_count; ++k) {
                        const long *indices = single->array_access.selection_indices + k * single->array_access.selection_tuple_size;
                        int error = 0;
                        const Value *element = array_value_get_element(source.array_value, indices, single->array_access.selection_tuple_size, &error);
                        if (element == NULL) {
                            fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                            for (size_t j = 0; j < k; ++j) {
                                value_free(&values[j]);
                            }
                            free(values);
                            value_free(&source);
                            return 1;
                        }
                        values[k] = value_copy(element);
                    }

                    ArrayValue *dynamic_array = array_value_create_dynamic_from_values(source.array_value->element_type, values, single->array_access.selection_count);
                    for (size_t k = 0; k < single->array_access.selection_count; ++k) {
                        value_free(&values[k]);
                    }
                    free(values);
                    value_free(&source);

                    if (dynamic_array == NULL) {
                        fprintf(stderr, "Error: out of memory while creating dynamic array\n");
                        return 1;
                    }

                    value = value_create_array(dynamic_array);
                }
            /* Array references on the right-hand side copy either one selected element or one indexed value. */
            } else if (single->is_assignment && single->array_access.name != NULL) {
                Value source;
                if (!environment_get(environment, single->array_access.name, &source)) {
                    fprintf(stderr, "Runtime Error: Undefined variable '%s'.\n", single->array_access.name);
                    return 1;
                }

                if (source.type != VALUE_ARRAY) {
                    fprintf(stderr, "Runtime Error: Variable '%s' is not an array.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                }

                if (single->array_access.is_selection) {
                    if (single->array_access.selection_count != 1) {
                        fprintf(stderr, "Runtime Error: Selection yields multiple values; declare an array variable with [].\n");
                        value_free(&source);
                        return 1;
                    }

                    if (single->array_access.selection_tuple_size != source.array_value->dimension_count) {
                        fprintf(stderr, "Runtime Error: Coordinate length %zu does not match array rank %ld.\n", single->array_access.selection_tuple_size, source.array_value->dimension_count);
                        value_free(&source);
                        return 1;
                    }

                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, single->array_access.selection_indices, single->array_access.selection_tuple_size, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        value_free(&source);
                        return 1;
                    }
                    value = value_copy(element);
                } else if (single->array_access.is_whole_array) {
                    fprintf(stderr, "Syntax Error: Array variable '%s' must be accessed explicitly.\n", single->array_access.name);
                    value_free(&source);
                    return 1;
                } else {
                    if (single->array_access.index_count == 0) {
                        fprintf(stderr, "Runtime Error: Invalid array access.\n");
                        value_free(&source);
                        return 1;
                    }

                    int error = 0;
                    const Value *element = array_value_get_element(source.array_value, single->array_access.indices, single->array_access.index_count, &error);
                    if (element == NULL) {
                        fprintf(stderr, "Runtime Error: Array index out of bounds.\n");
                        value_free(&source);
                        return 1;
                    }
                    value = value_copy(element);
                }

                value_free(&source);
            } else {
                value = single->value;
            }

            if (!environment_set(environment, single->name, &value)) {
                fprintf(stderr, "Runtime Error: Unable to declare variable '%s'.\n", single->name);
                value_free(&value);
                return 1;
            }
        }

        return 0;
    }

    return 1;
}
