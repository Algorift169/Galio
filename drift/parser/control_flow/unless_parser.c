/* SPDX-License-Identifier: AGPL-3.0-only */
/* Unless parsing stores one deferred condition and its indented body. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

static Token *parser_peek(Parser *parser)
{
    /* Inspect the current token without consuming it. */
    if (parser == NULL || parser->index >= parser->count) 
    {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

static Token *parser_advance(Parser *parser)
{
    /* Consume and return the current token. */
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}

static int is_statement_terminator(Token *token)
{
    /* Newlines and semicolons separate the header from the body. */
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static char *read_expression(Parser *parser)
{
    /* Rebuild the condition so the interpreter can evaluate it later. */
    char *result = NULL;
    size_t length = 0;

    while (parser_peek(parser) != NULL && parser_peek(parser)->type != TOKEN_COLON &&
           parser_peek(parser)->type != TOKEN_EOF && !is_statement_terminator(parser_peek(parser))) {
        Token *token = parser_advance(parser);
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

    if (result == NULL) {
        result = (char *)malloc(1U);
        if (result != NULL) {
            result[0] = '\0';
        }
    }
    return result;
}


static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    /* Grow the body list geometrically while preserving source order. */
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0U ? 4U : *capacity * 2U;
        Statement *new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building unless statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[(*count)++] = statement;
}

static void free_statement_list(Statement *body, size_t count)
{
    /* Release nested statements through their matching destructors. */
    if (body == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        switch (body[i].type) {
            case STATEMENT_PRINT: print_statement_free(&body[i].as.print_statement); break;
            case STATEMENT_VARIABLE_DECLARATION: variable_declaration_free(&body[i].as.variable_declaration); break;
            case STATEMENT_IF: if_statement_free(&body[i].as.if_statement); break;
            case STATEMENT_REPEAT: repeat_statement_free(&body[i].as.repeat_statement); break;
            case STATEMENT_FOR: for_statement_free(&body[i].as.for_statement); break;
            case STATEMENT_WHILE: while_statement_free(&body[i].as.while_statement); break;
            case STATEMENT_EACH: each_statement_free(&body[i].as.each_statement); break;
            case STATEMENT_UNLESS: unless_statement_free(&body[i].as.unless_statement); break;
            case STATEMENT_WHEN: when_statement_free(&body[i].as.when_statement); break;
            default: break;
        }
    }
    free(body);
}

int parse_unless_statement(Parser *parser, Statement *statement)
{
    /* Parse `unless condition:` until dedentation or an explicit end. */
    UnlessStatement unless_statement;
    Statement *body = NULL;
    size_t body_count = 0;
    size_t capacity = 0;
    size_t indentation;
    int inline_body = 1;

    if (parser == NULL || statement == NULL || parser_peek(parser) == NULL) {
        return 0;
    }
    memset(&unless_statement, 0, sizeof(unless_statement));
    indentation = parser_peek(parser)->indentation;
    parser_advance(parser);
    unless_statement.condition_text = read_expression(parser);
    if (unless_statement.condition_text == NULL || unless_statement.condition_text[0] == '\0') {
        fprintf(stderr, "Syntax Error: Expected condition after 'unless'.\n");
        free(unless_statement.condition_text);
        return 0;
    }
    if (parser_peek(parser) == NULL || parser_peek(parser)->type != TOKEN_COLON) {
        fprintf(stderr, "Syntax Error: Expected ':' after unless condition.\n");
        free(unless_statement.condition_text);
        return 0;
    }
    parser_advance(parser);

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
        append_statement(&body, &body_count, &capacity, parser_parse(parser));
        inline_body = 0;
    }
    if (parser_peek(parser) != NULL && (parser_peek(parser)->type == TOKEN_END || parser_peek(parser)->type == TOKEN_DOT)) {
        parser_advance(parser);
    }

    unless_statement.body = body;
    unless_statement.body_count = body_count;
    statement->type = STATEMENT_UNLESS;
    statement->as.unless_statement = unless_statement;
    return 1;
}

void unless_statement_free(UnlessStatement *statement)
{
    /* Release the deferred condition and recursively owned body. */
    if (statement == NULL) {
        return;
    }
    free(statement->condition_text);
    free_statement_list(statement->body, statement->body_count);
    statement->condition_text = NULL;
    statement->body = NULL;
    statement->body_count = 0;
}
