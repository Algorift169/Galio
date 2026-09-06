/* SPDX-License-Identifier: AGPL-3.0-only */
/* Each parsing stores a range or array source and collects 
statements until end. Each loops are parsed at runtime to support dynamic
expressions and array iteration. Here the range operator is implemented as
a half-open interval, so the end value is excluded from the iteration.  It 
is exclusive to match the behavior of the range operator in other languages,
and to allow for easy iteration over zero-based indices.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

// Parses an each statement and its body, storing the loop variable name, source text, 
// and body statements in the EachStatement structure. The body is terminated by
// an 'end' keyword, a dot, or a dedent. The parser does not evaluate the source or execute 
// the body; that is done at runtime by the interpreter. The parser returns 1 on success,
// 0 on failure, and prints error messages to stderr.
static Token *parser_peek(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

// Advances the parser to the next token and returns the current token. Returns NULL if at 
// the end of the token array or if the parser is NULL. The parser does not own the tokens;
// the caller is responsible for freeing them after parsing is complete.
static Token *parser_advance(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}

static int is_statement_terminator(Token *token)
{
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

// Reads tokens from the parser until a stop token is encountered, a statement terminator is found, 
// or the end of the token array is reached. Returns a dynamically allocated string containing
// the concatenated token values, separated by spaces. The caller is responsible for freeing 
// the returned string. Returns NULL on memory allocation failure.
static char *read_tokens(Parser *parser, TokenType stop)
{
    char *result = NULL;
    size_t length = 0;

    // Read tokens until the stop token, a statement terminator, or EOF is reached.
    while (parser_peek(parser) != NULL && parser_peek(parser)->type != stop &&
           parser_peek(parser)->type != TOKEN_EOF && !is_statement_terminator(parser_peek(parser))) {
        Token *token = parser_advance(parser); // Consume the token and get its value.
        size_t token_length = token->value == NULL ? 0U : strlen(token->value);
        char *new_result = (char *)realloc(result, length + token_length + 2U);
        if (new_result == NULL) {
            free(result);
            return NULL;
        }
        result = new_result;
        if (length > 0U) {
            result[length++] = ' ';
        }
        if (token->value != NULL) {
            memcpy(result + length, token->value, token_length);
            length += token_length;
        }
        result[length] = '\0';
    }

    // If no tokens were read, return an empty string instead of NULL.
    if (result == NULL) {
        result = (char *)malloc(1U);
        if (result != NULL) {
            result[0] = '\0';
        }
    }
    return result;
}

// Appends a statement to a dynamically allocated array of statements, resizing the
// array if necessary. The caller is responsible for freeing the array and its contents.
static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0U ? 4U : *capacity * 2U; // formula: new_capacity = old_capacity * 2, starting from 4
        Statement *new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building each statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[(*count)++] = statement;
}

// Frees a dynamically allocated array of statements and their contents, including any nested statements.
static void free_statement_list(Statement *body, size_t count)
{
    if (body == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (body[i].type == STATEMENT_PRINT) {
            print_statement_free(&body[i].as.print_statement);
        } else if (body[i].type == STATEMENT_VARIABLE_DECLARATION) {
            variable_declaration_free(&body[i].as.variable_declaration);
        } else if (body[i].type == STATEMENT_IF) {
            if_statement_free(&body[i].as.if_statement);
        } else if (body[i].type == STATEMENT_REPEAT) {
            repeat_statement_free(&body[i].as.repeat_statement);
        } else if (body[i].type == STATEMENT_FOR) {
            for_statement_free(&body[i].as.for_statement);
        } else if (body[i].type == STATEMENT_WHILE) {
            while_statement_free(&body[i].as.while_statement);
        } else if (body[i].type == STATEMENT_EACH) {
            each_statement_free(&body[i].as.each_statement);
            } else if (body[i].type == STATEMENT_UNLESS) {
                unless_statement_free(&body[i].as.unless_statement);
            } else if (body[i].type == STATEMENT_WHEN) {
                when_statement_free(&body[i].as.when_statement);
        }
    }
    free(body);
}

// Parses the body of an each statement, collecting statements until a terminator is found.
static int parse_each_body(Parser *parser, size_t indentation, Statement **out_body, size_t *out_count)
{
    Statement *body = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int inline_body = 1;

    // To parse the body of the each statement, we continue parsing statements until we reach a 
    // terminator (end, dot, or dedent). Each statement is appended to the body array.
    while (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_EOF) {
        Token *token = parser_peek(parser);
        if (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON) {
            inline_body = 0;
            parser_advance(parser);
            continue;
        }
        if (token->type == TOKEN_END || token->type == TOKEN_DOT ||
            (!inline_body && token->indentation <= indentation)) {
            break;
        }
        append_statement(&body, &count, &capacity, parser_parse(parser));
        inline_body = 0;
    }
    *out_body = body;
    *out_count = count;
    return 1;
}

// Parses an each statement, including its loop variable, source expression, and body statements.
int parse_each_statement(Parser *parser, Statement *statement)
{
    EachStatement each_statement;
    Statement *body = NULL;
    size_t body_count = 0;
    size_t indentation;
    Token *token;

    if (parser == NULL || statement == NULL || parser_peek(parser) == NULL) {
        return 0;
    }
    // Memory for the EachStatement is initialized to zero to avoid uninitialized fields.
    memset(&each_statement, 0, sizeof(each_statement));
    indentation = parser_peek(parser)->indentation;
    parser_advance(parser);

    token = parser_peek(parser);
    if (token != NULL && token->type == TOKEN_VAR) {
        parser_advance(parser);
    }
    token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Syntax Error: Expected variable identifier after 'each'.\n");
        return 0;
    }
    each_statement.item_name = drift_duplicate_string(token->value);
    parser_advance(parser);

    token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IN) {
        fprintf(stderr, "Syntax Error: Expected 'in' after each variable.\n");
        free(each_statement.item_name);
        return 0;
    }
    parser_advance(parser); // Consume the 'in' token and move to the source expression.

    each_statement.source_text = read_tokens(parser, TOKEN_COLON);
    if (each_statement.source_text == NULL || each_statement.source_text[0] == '\0') {
        fprintf(stderr, "Syntax Error: Expected range or array after 'in'.\n");
        free(each_statement.item_name);
        free(each_statement.source_text);
        return 0;
    }

    // Check if the source text contains a range operator '...'. If so, split the 
    // source text into start and end parts, and mark the statement as a range. The start 
    // and end expressions will be evaluated at runtime, and the end value is exclusive.
    char *range = strstr(each_statement.source_text, "...");
    if (range != NULL) {
        *range = '\0';
        each_statement.start_text = each_statement.source_text;
        each_statement.end_text = range + 3;
        each_statement.is_range = 1;
    }
    if (parser_peek(parser) == NULL || parser_peek(parser)->type != TOKEN_COLON) {
        fprintf(stderr, "Syntax Error: Expected ':' after each source.\n");
        free(each_statement.source_text);
        free(each_statement.item_name);
        return 0;
    }
    parser_advance(parser);
    parse_each_body(parser, indentation, &body, &body_count);
    if (parser_peek(parser) != NULL && (parser_peek(parser)->type == TOKEN_END || parser_peek(parser)->type == TOKEN_DOT)) {
        parser_advance(parser);
    }

    each_statement.body = body;
    each_statement.body_count = body_count;
    statement->type = STATEMENT_EACH;
    statement->as.each_statement = each_statement;
    return 1;
}

// We do  
void each_statement_free(EachStatement *statement)
{
    if (statement == NULL) {
        return;
    }
    if (statement->is_range) {
        statement->start_text = NULL;
        statement->end_text = NULL;
    }
    free(statement->source_text);
    free(statement->item_name);
    free_statement_list(statement->body, statement->body_count);
    statement->source_text = NULL;
    statement->item_name = NULL;
    statement->body = NULL;
    statement->body_count = 0;
}