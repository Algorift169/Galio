/* SPDX-License-Identifier: AGPL-3.0-only */
/* When parsing stores a subject, ordered case values, and an 
optional else body. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

static Token *peek(Parser *parser)
{
    /* Inspect the current token without consuming it. */
    if (parser == NULL || parser->index >= parser->count) return NULL;
    return &parser->tokens[parser->index];
}

static Token *advance(Parser *parser)
{
    /* Consume and return the current token. */
    if (parser == NULL || parser->index >= parser->count) return NULL;
    return &parser->tokens[parser->index++];
}

static int terminator(Token *token)
{
    /* Newlines and semicolons separate headers from following statements. */
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static char *read_until(Parser *parser, TokenType stop)
{
    /* Rebuild a deferred expression until its structural delimiter. */
    char *result = NULL;
    size_t length = 0;
    while (peek(parser) != NULL && peek(parser)->type != stop && peek(parser)->type != TOKEN_EOF && !terminator(peek(parser))) {
        Token *token = advance(parser);
        size_t token_length = token->value == NULL ? 0U : strlen(token->value);
        char *new_result = (char *)realloc(result, length + token_length + 2U);
        if (new_result == NULL) {
            free(result);
            return NULL;
        }
        result = new_result;
        if (length > 0U) result[length++] = ' ';
        if (token->value != NULL) {
            memcpy(result + length, token->value, token_length);
            length += token_length;
        }
        result[length] = '\0';
    }
    if (result == NULL) {
        result = (char *)malloc(1U);
        if (result != NULL) result[0] = '\0';
    }
    return result;
}

static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    /* Grow a statement list while preserving source order. */
    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0U ? 4U : *capacity * 2U;
        Statement *new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building when statement body.\n");
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
    if (body == NULL) return;
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

static int parse_body(Parser *parser, size_t indentation, Statement **out_body, size_t *out_count)
{
    /* Collect statements deeper than the current case or else header. */
    Statement *body = NULL;
    size_t count = 0;
    size_t capacity = 0;
    while (peek(parser) != NULL && peek(parser)->type != TOKEN_EOF) {
        Token *token = peek(parser);
        if (terminator(token)) {
            advance(parser);
            continue;
        }
        if (token->type == TOKEN_END || token->type == TOKEN_DOT || token->indentation <= indentation) break;
        append_statement(&body, &count, &capacity, parser_parse(parser));
    }
    *out_body = body;
    *out_count = count;
    return 1;
}

int parse_when_statement(Parser *parser, Statement *statement)
{
    /* Parse `when subject:` followed by ordered cases and an optional else. */
    WhenStatement when_statement;
    size_t when_indentation;
    Token *token;
    if (parser == NULL || statement == NULL || peek(parser) == NULL) return 0;
    memset(&when_statement, 0, sizeof(when_statement));
    when_indentation = peek(parser)->indentation;
    advance(parser);
    when_statement.subject_text = read_until(parser, TOKEN_COLON);
    if (when_statement.subject_text == NULL || when_statement.subject_text[0] == '\0') {
        fprintf(stderr, "Syntax Error: Expected subject after 'when'.\n");
        free(when_statement.subject_text);
        return 0;
    }
    if (peek(parser) == NULL || peek(parser)->type != TOKEN_COLON) {
        fprintf(stderr, "Syntax Error: Expected ':' after when subject.\n");
        free(when_statement.subject_text);
        return 0;
    }
    advance(parser);

    while ((token = peek(parser)) != NULL && token->type != TOKEN_EOF && token->type != TOKEN_END && token->type != TOKEN_DOT) {
        if (terminator(token)) {
            advance(parser);
            continue;
        }
        if (token->indentation <= when_indentation) break;
        if (token->type == TOKEN_ELSE) {
            size_t else_indentation = token->indentation;
            Statement *else_body = NULL;
            size_t else_count = 0;
            advance(parser);
            if (peek(parser) == NULL || peek(parser)->type != TOKEN_COLON) {
                fprintf(stderr, "Syntax Error: Expected ':' after when else.\n");
                when_statement_free(&when_statement);
                return 0;
            }
            advance(parser);
            parse_body(parser, else_indentation, &else_body, &else_count);
            when_statement.else_body = else_body;
            when_statement.else_count = else_count;
            break;
        }

        WhenCase current_case;
        Statement *body = NULL;
        size_t body_count = 0;
        size_t case_indentation = token->indentation;
        memset(&current_case, 0, sizeof(current_case));
        current_case.value_text = read_until(parser, TOKEN_COLON);
        if (current_case.value_text == NULL || current_case.value_text[0] == '\0' || peek(parser) == NULL || peek(parser)->type != TOKEN_COLON) {
            fprintf(stderr, "Syntax Error: Expected case value followed by ':'.\n");
            free(current_case.value_text);
            when_statement_free(&when_statement);
            return 0;
        }
        advance(parser);
        parse_body(parser, case_indentation, &body, &body_count);
        current_case.body = body;
        current_case.body_count = body_count;
        WhenCase *new_cases = (WhenCase *)realloc(when_statement.cases, (when_statement.case_count + 1U) * sizeof(WhenCase));
        if (new_cases == NULL) {
            fprintf(stderr, "Error: out of memory while building when cases.\n");
            free(current_case.value_text);
            free_statement_list(body, body_count);
            when_statement_free(&when_statement);
            return 0;
        }
        when_statement.cases = new_cases;
        when_statement.cases[when_statement.case_count++] = current_case;
    }

    if (peek(parser) != NULL && (peek(parser)->type == TOKEN_END || peek(parser)->type == TOKEN_DOT)) {
        advance(parser);
    }
    statement->type = STATEMENT_WHEN;
    statement->as.when_statement = when_statement;
    return 1;
}

void when_statement_free(WhenStatement *statement)
{
    /* Release the subject, case values, case bodies, and optional else body. */
    if (statement == NULL) return;
    free(statement->subject_text);
    for (size_t i = 0; i < statement->case_count; ++i) {
        free(statement->cases[i].value_text);
        free_statement_list(statement->cases[i].body, statement->cases[i].body_count);
    }
    free(statement->cases);
    free_statement_list(statement->else_body, statement->else_count);
    statement->subject_text = NULL;
    statement->cases = NULL;
    statement->case_count = 0;
    statement->else_body = NULL;
    statement->else_count = 0;
}
