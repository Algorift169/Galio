/* SPDX-License-Identifier: AGPL-3.0-only */
/* Select parsing consumes indices recursively and leaves the following delimiter for its owning statement. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/array.h"
#include "drift/parser.h"
#include "drift/value.h"

static Token *parser_peek(Parser *parser);
static Token *parser_advance(Parser *parser);

/*
Checks the current token and consumes it when it matches the expected grammar
symbol. This keeps delimiter validation consistent across select() and tuple
parsing while allowing each caller to provide a precise error message.
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
Reads one flat comma-separated tuple of integer indices. For example, the
tokens 1, 2, 3 become one dynamically allocated list with tuple size three.
The closing parenthesis is left for the caller to consume.
*/
static int parse_integer_tuple(Parser *parser, long **out_indices, size_t *out_count)
{
    size_t count = 0;
    long *indices = NULL;

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected integer index in select() arguments.\n");
            free(indices);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        if (token->type != TOKEN_INTEGER) {
            fprintf(stderr, "Syntax Error: Expected integer index in select() arguments.\n");
            free(indices);
            return 0;
        }

        long index = 0;
        char *end = NULL;
        index = strtol(token->value, &end, 10);
        (void)end;

        parser_advance(parser);
        // Grow the list only after the token has been confirmed as an integer.
        long *new_indices = (long *)realloc(indices, (count + 1U) * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() indices\n");
            free(indices);
            return 0;
        }
        indices = new_indices;
        indices[count++] = index;

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (token != NULL && token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or ')' in select() arguments.\n");
        free(indices);
        return 0;
    }

    *out_indices = indices;
    *out_count = count;
    return 1;
}

/*
Parses a parenthesized group that may contain several tuples or nested groups.
Each child contributes one or more complete tuples; all children must have the
same tuple width before their flat index lists can be concatenated.
*/
static int parse_tuple_group(Parser *parser, long **out_indices, size_t *out_count, size_t *out_tuple_size)
{
    size_t tuple_count = 0;
    size_t tuple_size = 0;
    long *indices = NULL;

    if (!parser_expect(parser, TOKEN_LEFT_PAREN, "Syntax Error: Expected '(' in select() arguments.")) {
        return 0;
    }

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated tuple in select() arguments.\n");
            free(indices);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            parser_advance(parser);
            break;
        }

        long *child_indices = NULL;
        size_t child_tuple_count = 0;
        size_t child_tuple_size = 0;

        if (token->type == TOKEN_LEFT_PAREN) {
            // Nested groups allow select() arguments to be grouped recursively.
            if (!parse_tuple_group(parser, &child_indices, &child_tuple_count, &child_tuple_size)) {
                free(indices);
                return 0;
            }
        } else {
            // A non-parenthesized child is interpreted as one flat integer tuple.
            if (!parse_integer_tuple(parser, &child_indices, &child_tuple_size)) {
                free(indices);
                return 0;
            }
            child_tuple_count = 1U;
        }

        if (child_tuple_size == 0) {
            fprintf(stderr, "Syntax Error: Empty tuple in select() arguments.\n");
            free(indices);
            free(child_indices);
            return 0;
        }

        if (tuple_size == 0) {
            // The first child establishes the width expected from every sibling.
            tuple_size = child_tuple_size;
        } else if (child_tuple_size != tuple_size) {
            fprintf(stderr, "Syntax Error: Inconsistent select() tuple length.\n");
            free(indices);
            free(child_indices);
            return 0;
        }

        long *new_indices = (long *)realloc(indices, (tuple_count + child_tuple_count) * tuple_size * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            free(child_indices);
            return 0;
        }
        indices = new_indices;
        // Append child tuples in source order so runtime selection preserves ordering.
        memcpy(indices + tuple_count * tuple_size, child_indices, child_tuple_count * tuple_size * sizeof(long));
        tuple_count += child_tuple_count;
        free(child_indices);

        token = parser_peek(parser);
        if (token != NULL && token->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        break;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_PAREN, "Syntax Error: Expected ')' to close select() tuple.")) {
        free(indices);
        return 0;
    }

    *out_indices = indices;
    *out_count = tuple_count;
    *out_tuple_size = tuple_size;
    return 1;
}

/*
Parses the complete array.select(...) suffix. The direct integer form creates
one tuple, while grouped arguments can produce multiple tuples. The resulting
indices and break markers are stored in ArrayAccess for the interpreter.
*/
int parse_select_access(Parser *parser, ArrayAccess *access)
{
    if (access == NULL) {
        return 0;
    }

    if (!parser_expect(parser, TOKEN_DOT, "Syntax Error: Expected '.' before select().")) {
        return 0;
    }

    Token *token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER || strcmp(token->value, "select") != 0) {
        fprintf(stderr, "Syntax Error: Expected select() after array name.\n");
        return 0;
    }
    parser_advance(parser);
    // Require the function call's opening delimiter before reading its tuples.

    if (!parser_expect(parser, TOKEN_LEFT_PAREN, "Syntax Error: Expected '(' after select.")) {
        return 0;
    }

    size_t tuple_count = 0;
    size_t tuple_size = 0;
    long *indices = NULL;
    int *breaks = NULL;

    Token *first_argument = parser_peek(parser);
    if (first_argument != NULL && first_argument->type == TOKEN_INTEGER) {
        // Direct integer arguments are treated as one coordinate tuple.
        if (!parse_integer_tuple(parser, &indices, &tuple_size)) {
            return 0;
        }
        tuple_count = 1;
        breaks = (int *)calloc(1, sizeof(int));
        if (breaks == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            return 0;
        }
    } else while (1) {
        // Parenthesized groups may expand into multiple coordinate tuples.
        Token *token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated select() arguments.\n");
            free(indices);
            free(breaks);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        long *tuple = NULL;
        size_t tuple_len = 0;
        size_t child_tuple_size = 0;
        if (!parse_tuple_group(parser, &tuple, &tuple_len, &child_tuple_size)) {
            free(indices);
            free(breaks);
            return 0;
        }

        if (tuple_size == 0) {
            // The first group defines the coordinate width for the selection.
            tuple_size = child_tuple_size;
        } else if (child_tuple_size != tuple_size) {
            fprintf(stderr, "Syntax Error: Inconsistent select() tuple length.\n");
            free(tuple);
            free(indices);
            free(breaks);
            return 0;
        }

        long *new_indices = (long *)realloc(indices, (tuple_count + tuple_len) * tuple_size * sizeof(long));
        if (new_indices == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(tuple);
            free(indices);
            free(breaks);
            return 0;
        }
        indices = new_indices;
        // Copy the group's flat coordinates into the final selection buffer.
        memcpy(indices + tuple_count * tuple_size, tuple, tuple_len * tuple_size * sizeof(long));
        free(tuple);

        int *new_breaks = (int *)realloc(breaks, (tuple_count + tuple_len) * sizeof(int));
        if (new_breaks == NULL) {
            fprintf(stderr, "Error: out of memory while reading select() values\n");
            free(indices);
            free(breaks);
            return 0;
        }
        breaks = new_breaks;
        // Break metadata is initialized alongside each tuple for later consumers.
        for (size_t i = 0; i < tuple_len; ++i) {
            breaks[tuple_count + i] = 0;
        }
        tuple_count += tuple_len;

        Token *separator = parser_peek(parser);
        if (separator == NULL) {
            fprintf(stderr, "Syntax Error: Unterminated select() arguments.\n");
            free(indices);
            free(breaks);
            return 0;
        }

        if (separator->type == TOKEN_COMMA) {
            parser_advance(parser);
            continue;
        }

        if (separator->type == TOKEN_RIGHT_PAREN) {
            break;
        }

        fprintf(stderr, "Syntax Error: Expected ',' or ')' in select() arguments.\n");
        free(indices);
        free(breaks);
        return 0;
    }

    if (!parser_expect(parser, TOKEN_RIGHT_PAREN, "Syntax Error: Expected ')' after select() arguments.")) {
        free(indices);
        free(breaks);
        return 0;
    }

    access->is_selection = 1;
    access->selection_count = tuple_count;
    access->selection_tuple_size = tuple_size;
    access->selection_indices = indices;
    access->selection_breaks = breaks;
    // Transfer ownership of the parsed buffers to the access expression.
    return 1;
}

/*
Returns the current token without moving the parser cursor. NULL means the
cursor has reached the end of the token array.
*/
Token *parser_peek(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

/*
Returns the current token and advances the cursor by one. Parsing routines use
this only after validating the token with parser_peek or parser_expect.
*/
Token *parser_advance(Parser *parser)
{
    if (parser->index >= parser->count) {
        return NULL;
    }

    Token *token = &parser->tokens[parser->index];
    parser->index++;
    return token;
}
