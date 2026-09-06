/* SPDX-License-Identifier: AGPL-3.0-only */
/* Repeat parsing distinguishes range and infinite forms while preserving loop bounds for execution. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

static Token *parser_peek(Parser *parser)
{
    // Inspect the current token without consuming it.
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index];
}

static Token *parser_advance(Parser *parser)
{
    // Consume one token and return it to the caller.
    if (parser == NULL || parser->index >= parser->count) {
        return NULL;
    }
    return &parser->tokens[parser->index++];
}

static int parser_expect(Parser *parser, TokenType type, const char *message)
{
    /* Require one grammar token and advance only after validation succeeds. */
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
    // Header expressions stop at a newline or semicolon as well as explicit delimiters.
    return token != NULL && (token->type == TOKEN_NEWLINE || token->type == TOKEN_SEMICOLON);
}

static void append_statement(Statement **items, size_t *count, size_t *capacity, Statement statement)
{
    /* Grow the repeat body list as statements are parsed, preserving source order. */
    Statement *new_items;
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0U) ? 4U : (*capacity * 2U);
        new_items = (Statement *)realloc(*items, new_capacity * sizeof(Statement));
        if (new_items == NULL) {
            fprintf(stderr, "Error: out of memory while building repeat statement body.\n");
            return;
        }
        *items = new_items;
        *capacity = new_capacity;
    }
    (*items)[(*count)++] = statement;
}

static void free_statement_list(Statement *body, size_t count)
{
    /* Destroy each tagged statement with its matching cleanup routine before
       releasing the container list. */
    if (body == NULL) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (body[i].type == STATEMENT_PRINT) {
            print_statement_free(&body[i].as.print_statement);
        } else if (body[i].type == STATEMENT_VARIABLE_DECLARATION) {
            variable_declaration_free(&body[i].as.variable_declaration);
        } else if (body[i].type == STATEMENT_INPUT) {
            if (body[i].as.input_statement.items != NULL) {
                for (size_t j = 0; j < body[i].as.input_statement.count; ++j) {
                    free(body[i].as.input_statement.items[j].prompt);
                    free(body[i].as.input_statement.items[j].target_name);
                }
                free(body[i].as.input_statement.items);
                body[i].as.input_statement.items = NULL;
                body[i].as.input_statement.count = 0;
            }
        } else if (body[i].type == STATEMENT_IF) {
            if_statement_free(&body[i].as.if_statement);
        } else if (body[i].type == STATEMENT_REPEAT) {
            repeat_statement_free(&body[i].as.repeat_statement);
        } else if (body[i].type == STATEMENT_FOR) {
            for_statement_free(&body[i].as.for_statement);
        } else if (body[i].type == STATEMENT_WHILE) {
            while_statement_free(&body[i].as.while_statement);
        } else if (body[i].type == STATEMENT_EACH) {
            // The each_statement_free function is responsible for freeing the resources associated with
            // the EachStatement, including its body statements and any dynamically allocated strings.
            each_statement_free(&body[i].as.each_statement);
            } else if (body[i].type == STATEMENT_UNLESS) {
                unless_statement_free(&body[i].as.unless_statement);
            } else if (body[i].type == STATEMENT_WHEN) {
                when_statement_free(&body[i].as.when_statement);
        }
    }
    free(body);
}

static int is_identifier_valid(const char *name)
{
    /* Repeat counters use the language's identifier alphabet, including '_' and '-'. */
    size_t i;
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (!((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_' || name[0] == '-')) {
        return 0;
    }
    for (i = 1; name[i] != '\0'; ++i) {
        if (!((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') || name[i] == '_' || name[i] == '-')) {
            return 0;
        }
    }
    return 1;
}

static char *read_until_tokens(Parser *parser, TokenType stop_a, TokenType stop_b);

static char *read_until_token(Parser *parser, TokenType stop)
{
    // Convenience wrapper for expressions with one stopping token.
    return read_until_tokens(parser, stop, TOKEN_UNKNOWN);
}

static char *read_until_tokens(Parser *parser, TokenType stop_a, TokenType stop_b)
{
    /* Reconstruct a source-like expression until either delimiter. Quoted
       strings are preserved so the evaluator can parse the result later. */
    size_t length = 0;
    char *result = NULL;
    Token *token;

    while ((token = parser_peek(parser)) != NULL && token->type != stop_a && token->type != stop_b && token->type != TOKEN_EOF && !is_statement_terminator(token)) {
        size_t token_len = token->value != NULL ? strlen(token->value) : 0U;
        size_t extra = token_len + 2U;
        if (token->type == TOKEN_STRING && token->value != NULL && token->value[0] != '"' && token->value[0] != '\'') {
            extra += 2U;
        }

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
            if (token->type == TOKEN_STRING) {
                if (token->value[0] == '"' || token->value[0] == '\'') {
                    memcpy(result + length, token->value, strlen(token->value));
                    length += strlen(token->value);
                } else {
                    result[length++] = '"';
                    memcpy(result + length, token->value, strlen(token->value));
                    length += strlen(token->value);
                    result[length++] = '"';
                }
            } else {
                memcpy(result + length, token->value, strlen(token->value));
                length += strlen(token->value);
            }
            result[length] = '\0';
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

static int parse_repeat_body(Parser *parser, Statement **out_body, size_t *out_count)
{
    /* Parse statements after the repeat header until 'end'. Nested statements
       consume their own bodies, so this loop resumes at the next sibling. */
    Statement *body = NULL;
    size_t body_count = 0;
    size_t capacity = 0;
    Token *token;

    if (out_body == NULL || out_count == NULL) {
        return 0;
    }
    *out_body = NULL;
    *out_count = 0;

    while (1) {
        token = parser_peek(parser);
        if (token == NULL || token->type == TOKEN_EOF) {
            break;
        }
        if (token->type == TOKEN_NEWLINE) {
            parser_advance(parser);
            continue;
        }
        if (token->type == TOKEN_END || token->type == TOKEN_DOT) {
            break;
        }

        Statement statement = parser_parse(parser);
        append_statement(&body, &body_count, &capacity, statement);
    }

    *out_body = body;
    *out_count = body_count;
    return 1;
}

int parse_repeat_statement(Parser *parser, Statement *statement)
{
    /*
    Build the repeat AST in stages: counter name, optional range syntax, body,
    and closing end token. Expressions remain as text because their values are
    evaluated later by the interpreter in the current environment.
    */
    RepeatStatement repeat_statement;
    Statement *body = NULL;
    size_t body_count = 0;
    Token *token;

    if (parser == NULL || statement == NULL) {
        return 0;
    }

    memset(statement, 0, sizeof(*statement));
    memset(&repeat_statement, 0, sizeof(repeat_statement));
    // The parser is positioned on 'repeat'; consume the keyword before reading the header.
    parser_advance(parser);

    token = parser_peek(parser);
    if (token == NULL) {
        fprintf(stderr, "Syntax Error: Expected loop variable in repeat declaration.\n");
        return 0;
    }

    if (token->type == TOKEN_INTEGER) {
        /* Compact count form: repeat N: body. The private counter preserves
           the normal repeat runtime while keeping the counter unnamed. */
        repeat_statement.counter_name = drift_duplicate_string("__repeat_counter");
        repeat_statement.declares_counter = 1;
        repeat_statement.has_range = 1;
        repeat_statement.is_exclusive_upper = 1;
        repeat_statement.start_text = drift_duplicate_string("0");
        repeat_statement.end_text = drift_duplicate_string(token->value);
        if (repeat_statement.counter_name == NULL || repeat_statement.start_text == NULL ||
            repeat_statement.end_text == NULL) {
            free(repeat_statement.counter_name);
            free(repeat_statement.start_text);
            free(repeat_statement.end_text);
            return 0;
        }
        parser_advance(parser);
        goto repeat_header_complete;
    }

    if (token->type == TOKEN_VAR) {
        // Both repeat counter declaration forms accept an optional 'var'.
        repeat_statement.declares_counter = 1;
        parser_advance(parser);
    }

    token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Syntax Error: Expected loop variable after 'repeat'.\n");
        return 0;
    }
    if (!is_identifier_valid(token->value)) {
        fprintf(stderr, "Syntax Error: Invalid identifier '%s'.\n", token->value);
        return 0;
    }
    repeat_statement.counter_name = drift_duplicate_string(token->value);
    parser_advance(parser);

    token = parser_peek(parser);
    if (token == NULL) {
        fprintf(stderr, "Syntax Error: Expected range or count after loop variable.\n");
        free(repeat_statement.counter_name);
        return 0;
    }

    if (token->type == TOKEN_LEFT_PAREN) {
        // Parentheses introduce either a finite range or the language's infinite form.
        parser_advance(parser);
        repeat_statement.has_range = 1;

        token = parser_peek(parser);
        if (token == NULL) {
            fprintf(stderr, "Syntax Error: Expected loop range or empty loop in repeat declaration.\n");
            free(repeat_statement.counter_name);
            return 0;
        }

        if (token->type == TOKEN_RIGHT_PAREN) {
            // An empty pair means an unbounded loop whose counter starts at runtime default.
            repeat_statement.is_infinite = 1;
            parser_advance(parser);
        } else if (token->type == TOKEN_RANGE) {
            // A range marker without endpoints is also treated as infinite syntax.
            repeat_statement.is_infinite = 1;
            parser_advance(parser);
            token = parser_peek(parser);
            if (token == NULL || token->type != TOKEN_RIGHT_PAREN) {
                fprintf(stderr, "Syntax Error: Expected ')' after loop range.\n");
                free(repeat_statement.counter_name);
                return 0;
            }
            parser_advance(parser);
        } else {
            repeat_statement.start_text = read_until_tokens(parser, TOKEN_RANGE, TOKEN_DOT);
            // Keep start/end/step expressions as text for runtime evaluation.
            if (repeat_statement.start_text == NULL) {
                fprintf(stderr, "Error: out of memory while reading loop start.\n");
                free(repeat_statement.counter_name);
                return 0;
            }

            token = parser_peek(parser);
            if (token == NULL) {
                fprintf(stderr, "Syntax Error: Expected range operator after loop start.\n");
                free(repeat_statement.start_text);
                free(repeat_statement.counter_name);
                return 0;
            }

            if (token->type == TOKEN_RANGE) {
                // '..' is the inclusive range operator.
                parser_advance(parser);
            } else if (token->type == TOKEN_DOT) {
                // Three-dot forms are followed by '<' or '>' for exclusive bounds.
                parser_advance(parser);
                token = parser_peek(parser);
                if (token != NULL && token->type == TOKEN_DOT) {
                    parser_advance(parser);
                    token = parser_peek(parser);
                    if (token != NULL && token->type == TOKEN_LESS) {
                        repeat_statement.is_exclusive_upper = 1;
                        parser_advance(parser);
                    } else if (token != NULL && token->type == TOKEN_GREATER) {
                        repeat_statement.is_exclusive_lower = 1;
                        parser_advance(parser);
                    } else {
                        fprintf(stderr, "Syntax Error: Expected '<' or '>' after '..' in loop range.\n");
                        free(repeat_statement.start_text);
                        free(repeat_statement.counter_name);
                        return 0;
                    }
                } else {
                    fprintf(stderr, "Syntax Error: Expected '..' or '...' in loop range.\n");
                    free(repeat_statement.start_text);
                    free(repeat_statement.counter_name);
                    return 0;
                }
            } else {
                fprintf(stderr, "Syntax Error: Expected range operator after loop start.\n");
                free(repeat_statement.start_text);
                free(repeat_statement.counter_name);
                return 0;
            }

            repeat_statement.end_text = read_until_tokens(parser, TOKEN_COMMA, TOKEN_RIGHT_PAREN);
            if (repeat_statement.end_text == NULL) {
                fprintf(stderr, "Error: out of memory while reading loop end.\n");
                free(repeat_statement.start_text);
                free(repeat_statement.counter_name);
                return 0;
            }

            token = parser_peek(parser);
            if (token != NULL && token->type == TOKEN_COMMA) {
                // A comma introduces the optional counter step expression.
                parser_advance(parser);
                repeat_statement.has_step = 1;
                repeat_statement.step_text = read_until_token(parser, TOKEN_RIGHT_PAREN);
                if (repeat_statement.step_text == NULL) {
                    fprintf(stderr, "Error: out of memory while reading loop step.\n");
                    free(repeat_statement.start_text);
                    free(repeat_statement.end_text);
                    free(repeat_statement.counter_name);
                    return 0;
                }
            }

            token = parser_peek(parser);
            if (token == NULL || token->type != TOKEN_RIGHT_PAREN) {
                fprintf(stderr, "Syntax Error: Expected ')' after loop range.\n");
                free(repeat_statement.start_text);
                free(repeat_statement.end_text);
                free(repeat_statement.step_text);
                free(repeat_statement.counter_name);
                return 0;
            }
            parser_advance(parser);
        }
    } else if (token->type == TOKEN_COLON) {
        // A colon-only header is recognized but has no executable range.
        repeat_statement.has_range = 0;
        repeat_statement.has_step = 0;
        parser_advance(parser);
    } else {
        fprintf(stderr, "Syntax Error: Expected '(' or ':' after loop variable.\n");
        free(repeat_statement.counter_name);
        return 0;
    }

repeat_header_complete:
    token = parser_peek(parser);
    if (token == NULL || token->type != TOKEN_COLON) {
        fprintf(stderr, "Syntax Error: Expected ':' after repeat header.\n");
        free(repeat_statement.counter_name);
        free(repeat_statement.start_text);
        free(repeat_statement.end_text);
        free(repeat_statement.step_text);
        return 0;
    }
    parser_advance(parser);

    // The body is parsed only after the complete header has been validated.
    if (!parse_repeat_body(parser, &body, &body_count)) {
        free(repeat_statement.counter_name);
        free(repeat_statement.start_text);
        free(repeat_statement.end_text);
        free(repeat_statement.step_text);
        return 0;
    }

    if (parser_peek(parser) != NULL &&
        (parser_peek(parser)->type == TOKEN_END || parser_peek(parser)->type == TOKEN_DOT)) {
        // `.` is the preferred block terminator; `end` remains valid for compatibility.
        parser_advance(parser);
    }

    repeat_statement.body = body;
    repeat_statement.body_count = body_count;
    statement->type = STATEMENT_REPEAT;
    statement->as.repeat_statement = repeat_statement;
    return 1;
}

void repeat_statement_free(RepeatStatement *statement)
{
    /* Free all expression strings and recursively destroy the parsed body so
       nested loops and conditionals do not retain allocated statements. */
    if (statement == NULL) {
        return;
    }

    free(statement->counter_name);
    statement->counter_name = NULL;
    free(statement->start_text);
    statement->start_text = NULL;
    free(statement->end_text);
    statement->end_text = NULL;
    free(statement->step_text);
    statement->step_text = NULL;

    if (statement->body != NULL) {
        free_statement_list(statement->body, statement->body_count);
        statement->body = NULL;
        statement->body_count = 0;
    }
}
