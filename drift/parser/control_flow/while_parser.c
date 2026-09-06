/* SPDX-License-Identifier: AGPL-3.0-only */
/* While parsing stores a deferred condition and collects statements until the matching end. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

static Token *parser_peek(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

static Token *parser_advance(Parser *parser)
{
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}

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
static int is_statement_terminator(Token *token)
{
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    /* Grow the body list geometrically so nested statements remain in source order. */
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0U) ? 4U : (*capacity * 2U);
        Statement *new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building while statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[(*count)++] = statement;
}

static void free_statement_list(Statement *body, size_t count)
{
    /* Free each tagged child with its matching destructor before freeing the body list. */
    if (body == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (body[i].type == STATEMENT_PRINT) {
            print_statement_free(&body[i].as.print_statement);
        } else if (body[i].type == STATEMENT_VARIABLE_DECLARATION) {
            variable_declaration_free(&body[i].as.variable_declaration);
        } else if (body[i].type == STATEMENT_INPUT) {
            for (size_t j = 0; j < body[i].as.input_statement.count; ++j) {
                free(body[i].as.input_statement.items[j].prompt);
                free(body[i].as.input_statement.items[j].target_name);
            }
            free(body[i].as.input_statement.items);
        } else if (body[i].type == STATEMENT_IF) {
            if_statement_free(&body[i].as.if_statement);
        } else if (body[i].type == STATEMENT_REPEAT) {
            repeat_statement_free(&body[i].as.repeat_statement);
        } else if (body[i].type == STATEMENT_FOR) {
            for_statement_free(&body[i].as.for_statement);
        } else if (body[i].type == STATEMENT_WHILE) {
            while_statement_free(&body[i].as.while_statement);
        } else if (body[i].type == STATEMENT_EACH) {
            // The each_statement_free function is responsible for freeing the resources associated 
            // with the EachStatement, including its body statements and any dynamically allocated strings.
            each_statement_free(&body[i].as.each_statement);
            } else if (body[i].type == STATEMENT_UNLESS) {
                unless_statement_free(&body[i].as.unless_statement);
            } else if (body[i].type == STATEMENT_WHEN) {
                when_statement_free(&body[i].as.when_statement);
        }
    }
    free(body);
}

static char *read_condition(Parser *parser)
{
    /* Rebuild the condition until ':'; runtime evaluation will tokenize it again. */
    char *result = NULL;
    size_t length = 0;
    while (parser_peek(parser) != NULL) {
        Token *token = parser_peek(parser);
        if (token->type == TOKEN_COLON || token->type == TOKEN_EOF || is_statement_terminator(token)) {
            break;
        }
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
        parser_advance(parser);
    }
    if (result == NULL) {
        result = (char *)malloc(1U);
        if (result != NULL) {
            result[0] = '\0';
        }
    }
    return result;
}

int parse_while_statement(Parser *parser, Statement *statement)
{
    /*
    Parse `while condition:` and retain the condition as text. The body is
    parsed into statements until `end`, allowing nested loops and conditionals.
    */
    WhileStatement while_statement;
    Statement *body = NULL;
    size_t body_count = 0;
    size_t body_capacity = 0;
    size_t block_indentation;
    int inline_body = 1;

    if (parser == NULL || statement == NULL) {
        return 0;
    }
    memset(&while_statement, 0, sizeof(while_statement));
    block_indentation = parser_peek(parser)->indentation;
    parser_advance(parser);

    while_statement.condition_text = read_condition(parser);
    if (while_statement.condition_text == NULL || while_statement.condition_text[0] == '\0') {
        fprintf(stderr, "Syntax Error: Expected condition after 'while'.\n");
        free(while_statement.condition_text);
        return 0;
    }
    if (!parser_expect(parser, TOKEN_COLON, "Syntax Error: Expected ':' after while condition.")) {
        free(while_statement.condition_text);
        return 0;
    }

    while (1) {
        Token *token = parser_peek(parser);
        if (token == NULL || token->type == TOKEN_EOF) {
            break;
        }
        if (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON) {
            inline_body = 0;
            parser_advance(parser);
            continue;
        }
        if (token->type == TOKEN_END || token->type == TOKEN_DOT ||
            (!inline_body && token->indentation <= block_indentation)) {
            break;
        }
        append_statement(&body, &body_count, &body_capacity, parser_parse(parser));
        inline_body = 0;
    }

    if (parser_peek(parser) != NULL &&
        (parser_peek(parser)->type == TOKEN_END || parser_peek(parser)->type == TOKEN_DOT)) {
        // `.` is the preferred block terminator; `end` remains valid for compatibility.
        parser_advance(parser);
    }

    while_statement.body = body;
    while_statement.body_count = body_count;
    statement->type = STATEMENT_WHILE;
    statement->as.while_statement = while_statement;
    return 1;
}

void while_statement_free(WhileStatement *statement)
{
    if (statement == NULL) {
        return;
    }
    free(statement->condition_text);
    free_statement_list(statement->body, statement->body_count);
    statement->condition_text = NULL;
    statement->body = NULL;
    statement->body_count = 0;
}
