/* SPDX-License-Identifier: AGPL-3.0-only */
/* For parsing stores initialization, condition, increment, and body separately for runtime execution. */

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
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0U) ? 4U : (*capacity * 2U);
        Statement *new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building for statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[(*count)++] = statement;
}

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
            // The while_statement_free function is responsible for freeing the resources associated
            // with the WhileStatement, including its body statements and any dynamically allocated strings.
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

static char *read_clause(Parser *parser, TokenType stop)
{
    /*
    Rebuild one for clause from tokens until its comma or closing parenthesis.
    Parentheses and brackets are tracked so delimiters inside an expression do
    not accidentally terminate the clause.
    */
    char *result = NULL;
    size_t length = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    while (parser_peek(parser) != NULL) {
        Token *token = parser_peek(parser);
        if (token->type == stop && paren_depth == 0 && bracket_depth == 0) {
            break;
        }
        if (token->type == TOKEN_EOF || is_statement_terminator(token)) {
            break;
        }

        size_t token_length = token->value == NULL ? 0U : strlen(token->value);
        size_t extra = token_length + (length == 0U ? 1U : 2U);
        char *new_result = (char *)realloc(result, length + extra);
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

        if (token->type == TOKEN_LEFT_PAREN) {
            paren_depth++;
        } else if (token->type == TOKEN_RIGHT_PAREN && paren_depth > 0) {
            paren_depth--;
        } else if (token->type == TOKEN_LEFT_BRACKET) {
            bracket_depth++;
        } else if (token->type == TOKEN_RIGHT_BRACKET && bracket_depth > 0) {
            bracket_depth--;
        }
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

static int parse_for_body(Parser *parser, size_t block_indentation, Statement **out_body, size_t *out_count)
{
    /* The body parser leaves 'end' for parse_for_statement to consume. */
    Statement *body = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int inline_body = 1;

    if (out_body == NULL || out_count == NULL) {
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
        append_statement(&body, &count, &capacity, parser_parse(parser));
        inline_body = 0;
    }
    *out_body = body;
    *out_count = count;
    return 1;
}

int parse_for_statement(Parser *parser, Statement *statement)
{
    /*
    Parse `for init, condition, increment:`. Clauses stay as text because the
    initialization and increment must run each time the statement executes,
    while the condition must be reevaluated before every iteration.
    */
    ForStatement for_statement;
    Statement *body = NULL;
    size_t body_count = 0;
    size_t block_indentation;

    if (parser == NULL || statement == NULL) {
        return 0;
    }
    memset(&for_statement, 0, sizeof(for_statement));
    block_indentation = parser_peek(parser)->indentation;
    parser_advance(parser);

    int has_parentheses = 0;
    if (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_LEFT_PAREN) {
        has_parentheses = 1;
        parser_advance(parser);
    }
    if (parser_peek(parser) != NULL && parser_peek(parser)->type == TOKEN_VAR) {
        // `var` explicitly declares the counter; without it the counter must already exist.
        for_statement.declares_counter = 1;
        parser_advance(parser);
    }
    for_statement.init_text = read_clause(parser, TOKEN_COMMA);
    if (for_statement.init_text == NULL || !parser_expect(parser, TOKEN_COMMA, "Syntax Error: Expected ',' after for initialization.")) {
        free(for_statement.init_text);
        return 0;
    }
    for_statement.condition_text = read_clause(parser, TOKEN_COMMA);
    if (for_statement.condition_text == NULL || !parser_expect(parser, TOKEN_COMMA, "Syntax Error: Expected ',' after for condition.")) {
        free(for_statement.init_text);
        free(for_statement.condition_text);
        return 0;
    }
    for_statement.increment_text = read_clause(parser, has_parentheses ? TOKEN_RIGHT_PAREN : TOKEN_COLON);
    if (for_statement.increment_text == NULL ||
        (has_parentheses && !parser_expect(parser, TOKEN_RIGHT_PAREN, "Syntax Error: Expected ')' after for increment."))) {
        free(for_statement.init_text);
        free(for_statement.condition_text);
        free(for_statement.increment_text);
        return 0;
    }
    if (!parser_expect(parser, TOKEN_COLON, "Syntax Error: Expected ':' after for header.")) {
        free(for_statement.init_text);
        free(for_statement.condition_text);
        free(for_statement.increment_text);
        return 0;
    }
    parse_for_body(parser, block_indentation, &body, &body_count);
    if (parser_peek(parser) != NULL &&
        (parser_peek(parser)->type == TOKEN_END || parser_peek(parser)->type == TOKEN_DOT)) {
        // `.` is the preferred block terminator; `end` remains valid for compatibility.
        parser_advance(parser);
    }

    for_statement.body = body;
    for_statement.body_count = body_count;
    statement->type = STATEMENT_FOR;
    statement->as.for_statement = for_statement;
    return 1;
}

void for_statement_free(ForStatement *statement)
{
    if (statement == NULL) {
        return;
    }
    free(statement->init_text);
    free(statement->condition_text);
    free(statement->increment_text);
    free_statement_list(statement->body, statement->body_count);
    statement->init_text = NULL;
    statement->condition_text = NULL;
    statement->increment_text = NULL;
    statement->body = NULL;
    statement->body_count = 0;
}
