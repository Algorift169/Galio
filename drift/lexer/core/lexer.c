/* SPDX-License-Identifier: AGPL-3.0-only */
/* The lexer owns its cursor and emits one token at a time; callers release token-owned text after parsing. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/comments.h"
#include "drift/identity_keywords.h"
#include "drift/logical_keywords.h"
#include "drift/lexer.h"

/* Identifies horizontal whitespace that can separate tokens without ending a line. */
static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* Identifies line boundaries so token locations and statement separation stay aligned. */
static int is_newline(char c)
{
    return c == '\n';
}

/* Checks whether a character may begin a Drift identifier. */
static int is_identifier_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

/* Checks whether a character may continue an identifier after its first character. */
static int is_identifier_part(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

/* Packages a token type with already-owned text without duplicating that text. */
static Token make_token(TokenType type, char *value)
{
    Token token;
    token.type = type;
    token.value = value;
    return token;
}

/* Copies source text and initializes all cursor and comment-tracking fields. */
Lexer lexer_create(const char *source)
{
    Lexer lexer;
    lexer.source = source;
    lexer.index = 0;
    lexer.length = 0;
    lexer.in_block_comment = 0;
    lexer.in_block_comment_code = 0;
    lexer.at_line_start = 1;
    lexer.line_indentation = 0;

    if (source != NULL) {
        while (source[lexer.length] != '\0') {
            lexer.length++;
        }
    }

    return lexer;
}

/* Advances past separators while preserving newlines needed by the parser. */
static void skip_whitespace(Lexer *lexer)
{
    if (lexer->at_line_start) {
        lexer->line_indentation = 0;
        while (lexer->index < lexer->length && (lexer->source[lexer->index] == ' ' || lexer->source[lexer->index] == '\t')) {
            lexer->index++;
            lexer->line_indentation++;
        }
        lexer->at_line_start = 0;
    }
    while (lexer->index < lexer->length && is_space(lexer->source[lexer->index])) {
        lexer->index++;
    }
}

/* Consumes an identifier, then maps reserved spellings to keyword tokens. */
static Token read_identifier(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;

    if (!is_identifier_start(lexer->source[lexer->index])) {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    while (lexer->index < lexer->length && is_identifier_part(lexer->source[lexer->index])) {
        /* A trailing -- or -= belongs to the operator stream, not to a
           hyphenated identifier such as user-name. */
        if (lexer->source[lexer->index] == '-' && lexer->index + 1 < lexer->length &&
            (lexer->source[lexer->index + 1] == '-' || lexer->source[lexer->index + 1] == '=')) {
            break;
        }
        lexer->index++;
        length++;
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading identifier\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    for (size_t i = 0; i < length; ++i) {
        value[i] = lexer->source[start + i];
    }
    value[length] = '\0';

    if (strcmp(value, "say") == 0) {
        free(value);
        return make_token(TOKEN_SAY, drift_duplicate_string("say"));
    }

    if (strcmp(value, "ask") == 0) {
        free(value);
        return make_token(TOKEN_ASK, drift_duplicate_string("ask"));
    }

    if (strcmp(value, "var") == 0) {
        free(value);
        return make_token(TOKEN_VAR, drift_duplicate_string("var"));
    }

    if (strcmp(value, "if") == 0) {
        free(value);
        return make_token(TOKEN_IF, drift_duplicate_string("if"));
    }

    if (strcmp(value, "elif") == 0) {
        free(value);
        return make_token(TOKEN_ELIF, drift_duplicate_string("elif"));
    }

    if (strcmp(value, "else") == 0) {
        free(value);
        return make_token(TOKEN_ELSE, drift_duplicate_string("else"));
    }

    if (strcmp(value, "repeat") == 0) {
        free(value);
        return make_token(TOKEN_REPEAT, drift_duplicate_string("repeat"));
    }

    if (strcmp(value, "for") == 0) {
        free(value);
        return make_token(TOKEN_FOR, drift_duplicate_string("for"));
    }

    if (strcmp(value, "while") == 0) {
        free(value);
        return make_token(TOKEN_WHILE, drift_duplicate_string("while"));
    }

    if (strcmp(value, "unless") == 0) {
        free(value);
        return make_token(TOKEN_UNLESS, drift_duplicate_string("unless"));
    }

    if (strcmp(value, "when") == 0) {
        free(value);
        return make_token(TOKEN_WHEN, drift_duplicate_string("when"));
    }

    if (strcmp(value, "fun") == 0) {
        free(value);
        return make_token(TOKEN_FUN, drift_duplicate_string("fun"));
    }

    if (strcmp(value, "return") == 0) {
        free(value);
        return make_token(TOKEN_RETURN, drift_duplicate_string("return"));
    }

    if (strcmp(value, "break") == 0) {
        free(value);
        return make_token(TOKEN_BREAK, drift_duplicate_string("break"));
    }

    if (strcmp(value, "continue") == 0) {
        free(value);
        return make_token(TOKEN_CONTINUE, drift_duplicate_string("continue"));
    }

    // The following keywords are added to support the "each" loop construct in the Drift programming
    // language. The "each" keyword is used to iterate over elements in a collection, and the 
    // "in" keyword is used to specify the collection being iterated over. The "end" keyword is 
    // used to mark the end of the each loop.
    if (strcmp(value, "each") == 0) {
        free(value);
        return make_token(TOKEN_EACH, drift_duplicate_string("each"));
    }

    if (strcmp(value, "in") == 0) {
        free(value);
        return make_token(TOKEN_IN, drift_duplicate_string("in"));
    }

    if (strcmp(value, "end") == 0) {
        free(value);
        return make_token(TOKEN_END, drift_duplicate_string("end"));
    }

    if (strcmp(value, "true") == 0 || strcmp(value, "TRUE") == 0) {
        free(value);
        return make_token(TOKEN_TRUE, drift_duplicate_string("true"));
    }

    if (strcmp(value, "false") == 0 || strcmp(value, "FALSE") == 0) {
        free(value);
        return make_token(TOKEN_FALSE, drift_duplicate_string("false"));
    }

    if (strcmp(value, "NULL") == 0 || strcmp(value, "null") == 0) {
        free(value);
        return make_token(TOKEN_NULL, drift_duplicate_string("NULL"));
    }

    if (strcmp(value, "INF") == 0 || strcmp(value, "inf") == 0) {
        free(value);
        return make_token(TOKEN_INFINITY, drift_duplicate_string("INF"));
    }

    TokenType logical_type = logical_keyword_token_type(value);
    if (logical_type != TOKEN_UNKNOWN) {
        return make_token(logical_type, value);
    }

    TokenType identity_type = identity_keyword_token_type(value);
    if (identity_type != TOKEN_UNKNOWN) {
        return make_token(identity_type, value);
    }

    return make_token(TOKEN_IDENTIFIER, value);
}

/* Consumes an integer or decimal literal and rejects malformed numeric text. */
static Token read_number(Lexer *lexer)
{
    size_t start = lexer->index;
    int has_decimal = 0;
    char *value;
    size_t length = 0;

    while (lexer->index < lexer->length) {
        char c = lexer->source[lexer->index];
        if (isdigit((unsigned char)c)) {
            lexer->index++;
            length++;
            continue;
        }

        if (c == '.') {
            if (has_decimal) {
                fprintf(stderr, "Syntax Error: Invalid number literal.\n");
                return make_token(TOKEN_UNKNOWN, NULL);
            }
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '.') {
                break;
            }
            has_decimal = 1;
            lexer->index++;
            length++;
            continue;
        }

        break;
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading number\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    for (size_t i = 0; i < length; ++i) {
        value[i] = lexer->source[start + i];
    }
    value[length] = '\0';

    if (has_decimal) {
        return make_token(TOKEN_FLOAT, value);
    }

    return make_token(TOKEN_INTEGER, value);
}

/* Reads a single-quoted value using the language's quote and escape rules. */
static Token read_single_quoted_value(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;
    size_t i;
    size_t out_index = 0;

    if (lexer->index >= lexer->length || lexer->source[lexer->index] != '\'') {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    lexer->index++;
    while (lexer->index < lexer->length && lexer->source[lexer->index] != '\'') {
        if (lexer->source[lexer->index] == '\n') {
            fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
            return make_token(TOKEN_UNKNOWN, NULL);
        }
        lexer->index++;
        length++;
    }

    if (lexer->index >= lexer->length) {
        fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value = (char *)malloc(length + 3U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading single-quoted value\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value[0] = '\'';
    i = start + 1;
    while (i < lexer->index) {
        value[out_index + 1] = lexer->source[i];
        out_index++;
        i++;
    }
    value[out_index + 1] = '\'';
    value[out_index + 2] = '\0';

    lexer->index++;
    return make_token(TOKEN_STRING, value);
}

/* Reads a double-quoted string while advancing over escapes and its closing quote. */
static Token read_string(Lexer *lexer)
{
    size_t start = lexer->index;
    size_t length = 0;
    char *value;
    size_t i;
    size_t out_index = 0;

    if (lexer->index >= lexer->length || lexer->source[lexer->index] != '"') {
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    lexer->index++;
    while (lexer->index < lexer->length && lexer->source[lexer->index] != '"') {
        if (lexer->source[lexer->index] == '\n') {
            fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
            return make_token(TOKEN_UNKNOWN, NULL);
        }

        if (lexer->source[lexer->index] == '\\' && lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == 'n') {
            length++;
            lexer->index += 2;
            continue;
        }

        lexer->index++;
        length++;
    }

    if (lexer->index >= lexer->length) {
        fprintf(stderr, "Syntax Error: Unterminated string literal.\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    value = (char *)malloc(length + 1U);
    if (value == NULL) {
        fprintf(stderr, "Error: out of memory while reading string\n");
        return make_token(TOKEN_UNKNOWN, NULL);
    }

    i = start + 1;
    while (i < lexer->index) {
        if (lexer->source[i] == '\\' && i + 1 < lexer->index && lexer->source[i + 1] == 'n') {
            value[out_index++] = '\n';
            i += 2;
        } else {
            value[out_index++] = lexer->source[i];
            i++;
        }
    }
    value[out_index] = '\0';

    lexer->index++;
    return make_token(TOKEN_STRING, value);
}

Token *lexer_scan_all(Lexer *lexer, size_t *token_count)
{
    Token *tokens = NULL;
    size_t capacity = 16;
    size_t count = 0;

    if (token_count != NULL) {
        *token_count = 0;
    }

    tokens = (Token *)malloc(capacity * sizeof(Token));
    if (tokens == NULL) {
        fprintf(stderr, "Error: out of memory while allocating tokens\n");
        return NULL;
    }

    while (lexer->index < lexer->length) {
        skip_whitespace(lexer);

        int comment_res = lexer_skip_comments(lexer);
        if (comment_res < 0) {
            token_free_array(tokens, count);
            return NULL;
        }
        if (comment_res > 0) {
            continue;
        }

        if (lexer->index >= lexer->length) {
            break;
        }

        if (is_newline(lexer->source[lexer->index])) {
            tokens[count++] = make_token(TOKEN_NEWLINE, NULL);
            lexer->index++;
            tokens[count - 1U].indentation = lexer->line_indentation;
            lexer->at_line_start = 1;
            lexer->line_indentation = 0;
            if (count >= capacity) {
                Token *new_tokens;
                capacity *= 2;
                new_tokens = (Token *)realloc(tokens, capacity * sizeof(Token));
                if (new_tokens == NULL) {
                    fprintf(stderr, "Error: out of memory while growing token array\n");
                    free(tokens);
                    return NULL;
                }
                tokens = new_tokens;
            }
            continue;
        }

        if (lexer->source[lexer->index] == '=') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_EQUAL_EQUAL, drift_duplicate_string("=="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_EQUAL, drift_duplicate_string("="));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '+') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '+') {
                tokens[count++] = make_token(TOKEN_PLUS_PLUS, drift_duplicate_string("++"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PLUS_EQUAL, drift_duplicate_string("+="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PLUS, drift_duplicate_string("+"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '-') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '-') {
                tokens[count++] = make_token(TOKEN_MINUS_MINUS, drift_duplicate_string("--"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_MINUS_EQUAL, drift_duplicate_string("-="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_MINUS, drift_duplicate_string("-"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '*') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_STAR_EQUAL, drift_duplicate_string("*="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_STAR, drift_duplicate_string("*"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '/') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_SLASH_EQUAL, drift_duplicate_string("/="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_SLASH, drift_duplicate_string("/"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '%') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PERCENT_EQUAL, drift_duplicate_string("%="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PERCENT, drift_duplicate_string("%"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '!') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_NOT_EQUAL, drift_duplicate_string("!="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_BANG, drift_duplicate_string("!"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '<') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT_EQUAL, drift_duplicate_string("<<="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_LESS_EQUAL, drift_duplicate_string("<="));
                    lexer->index += 2;
                }
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '<') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT_EQUAL, drift_duplicate_string("<<="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_SHIFT_LEFT, drift_duplicate_string("<<"));
                    lexer->index += 2;
                }
            } else {
                tokens[count++] = make_token(TOKEN_LESS, drift_duplicate_string("<"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '>') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT_EQUAL, drift_duplicate_string(">>="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_GREATER_EQUAL, drift_duplicate_string(">="));
                    lexer->index += 2;
                }
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '>') {
                if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 2] == '=') {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT_EQUAL, drift_duplicate_string(">>="));
                    lexer->index += 3;
                } else {
                    tokens[count++] = make_token(TOKEN_SHIFT_RIGHT, drift_duplicate_string(">>"));
                    lexer->index += 2;
                }
            } else {
                tokens[count++] = make_token(TOKEN_GREATER, drift_duplicate_string(">"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '&') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '&') {
                tokens[count++] = make_token(TOKEN_AND_AND, drift_duplicate_string("&&"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_AMPERSAND_EQUAL, drift_duplicate_string("&="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_AMPERSAND, drift_duplicate_string("&"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '|') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '|') {
                tokens[count++] = make_token(TOKEN_OR_OR, drift_duplicate_string("||"));
                lexer->index += 2;
            } else if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_PIPE_EQUAL, drift_duplicate_string("|="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_PIPE, drift_duplicate_string("|"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '^') {
            if (lexer->index + 1 < lexer->length && lexer->source[lexer->index + 1] == '=') {
                tokens[count++] = make_token(TOKEN_CARET_EQUAL, drift_duplicate_string("^="));
                lexer->index += 2;
            } else {
                tokens[count++] = make_token(TOKEN_CARET, drift_duplicate_string("^"));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '~') {
            tokens[count++] = make_token(TOKEN_TILDA, drift_duplicate_string("~"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '?') {
            tokens[count++] = make_token(TOKEN_QUESTION, drift_duplicate_string("?"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '.') {
            if (lexer->index + 2 < lexer->length && lexer->source[lexer->index + 1] == '.' && lexer->source[lexer->index + 2] == '.') {
                tokens[count++] = make_token(TOKEN_RANGE, drift_duplicate_string("..."));
                lexer->index += 3;
            } else {
                tokens[count++] = make_token(TOKEN_DOT, drift_duplicate_string("."));
                lexer->index++;
            }
        } else if (lexer->source[lexer->index] == '@') {
            tokens[count++] = make_token(TOKEN_AT, drift_duplicate_string("@"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '{') {
            tokens[count++] = make_token(TOKEN_LEFT_BRACE, drift_duplicate_string("{"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '}') {
            tokens[count++] = make_token(TOKEN_RIGHT_BRACE, drift_duplicate_string("}"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '[') {
            tokens[count++] = make_token(TOKEN_LEFT_BRACKET, drift_duplicate_string("["));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ']') {
            tokens[count++] = make_token(TOKEN_RIGHT_BRACKET, drift_duplicate_string("]"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '(') {
            tokens[count++] = make_token(TOKEN_LEFT_PAREN, drift_duplicate_string("("));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ')') {
            tokens[count++] = make_token(TOKEN_RIGHT_PAREN, drift_duplicate_string(")"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ',') {
            tokens[count++] = make_token(TOKEN_COMMA, drift_duplicate_string(","));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ':') {
            tokens[count++] = make_token(TOKEN_COLON, drift_duplicate_string(":"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == ';') {
            tokens[count++] = make_token(TOKEN_SEMICOLON, drift_duplicate_string(";"));
            lexer->index++;
        } else if (lexer->source[lexer->index] == '\'') {
            Token token = read_single_quoted_value(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (lexer->source[lexer->index] == '"') {
            Token token = read_string(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (isdigit((unsigned char)lexer->source[lexer->index])) {
            Token token = read_number(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else if (is_identifier_start(lexer->source[lexer->index])) {
            Token token = read_identifier(lexer);
            if (token.type == TOKEN_UNKNOWN) {
                fprintf(stderr, "Syntax Error: Invalid identifier '%c'\n", lexer->source[lexer->index]);
                token_free_array(tokens, count);
                return NULL;
            }
            tokens[count++] = token;
        } else {
            fprintf(stderr, "Syntax Error: Unknown token '%c'\n", lexer->source[lexer->index]);
            token_free_array(tokens, count);
            return NULL;
        }

        tokens[count - 1U].indentation = lexer->line_indentation;

        if (count >= capacity) {
            Token *new_tokens;
            capacity *= 2;
            new_tokens = (Token *)realloc(tokens, capacity * sizeof(Token));
            if (new_tokens == NULL) {
                fprintf(stderr, "Error: out of memory while growing token array\n");
                token_free_array(tokens, count);
                return NULL;
            }
            tokens = new_tokens;
        }
    }

    if (lexer->in_block_comment) {
        fprintf(stderr, "Syntax Error: Unterminated block comment.\n");
        token_free_array(tokens, count);
        return NULL;
    }

    tokens[count++] = make_token(TOKEN_EOF, NULL);

    if (token_count != NULL) {
        *token_count = count;
    }

    return tokens;
}


/*
lexer full workflow graph is something like this:
lexer_scan_all()
│
├── Initialize token array
│
└── while (lexer->index < lexer->length)
    │
    ├── skip_whitespace()
    │   └── Skip: ' '  '\t'  '\r'
    │
    ├── lexer_skip_comments()
    │   │
    │   ├── No comment
    │   │   └── Continue lexical scanning
    │   │
    │   ├── Single-line comment
    │   │   └── Skip comment → continue loop
    │   │
    │   ├── Block comment
    │   │   └── Skip comment → continue loop
    │   │
    │   └── Comment error
    │       └── Return NULL
    │
    ├── Check end of source
    │   └── If reached → break
    │
    ├── Check newline
    │   └── TOKEN_NEWLINE
    │
    └── Token dispatch based on current character
        │
        ├── '='
        │   ├── '==' → TOKEN_EQUAL_EQUAL
        │   └── '='  → TOKEN_EQUAL
        │
        ├── '+'
        │   ├── '++' → TOKEN_PLUS_PLUS
        │   ├── '+=' → TOKEN_PLUS_EQUAL
        │   └── '+'  → TOKEN_PLUS
        │
        ├── '-'
        │   ├── '--' → TOKEN_MINUS_MINUS
        │   ├── '-=' → TOKEN_MINUS_EQUAL
        │   └── '-'  → TOKEN_MINUS
        │
        ├── '*'
        │   ├── '*=' → TOKEN_STAR_EQUAL
        │   └── '*'  → TOKEN_STAR
        │
        ├── '/'
        │   ├── '/=' → TOKEN_SLASH_EQUAL
        │   └── '/'  → TOKEN_SLASH
        │
        ├── '%'
        │   ├── '%=' → TOKEN_PERCENT_EQUAL
        │   └── '%'  → TOKEN_PERCENT
        │
        ├── '!'
        │   ├── '!=' → TOKEN_NOT_EQUAL
        │   └── '!'  → TOKEN_BANG
        │
        ├── '<'
        │   ├── '<='  → TOKEN_LESS_EQUAL
        │   ├── '<<'  → TOKEN_SHIFT_LEFT
        │   ├── '<<=' → TOKEN_SHIFT_LEFT_EQUAL
        │   └── '<'   → TOKEN_LESS
        │
        ├── '>'
        │   ├── '>='  → TOKEN_GREATER_EQUAL
        │   ├── '>>'  → TOKEN_SHIFT_RIGHT
        │   ├── '>>=' → TOKEN_SHIFT_RIGHT_EQUAL
        │   └── '>'   → TOKEN_GREATER
        │
        ├── '&'
        │   ├── '&&' → TOKEN_AND_AND
        │   ├── '&=' → TOKEN_AMPERSAND_EQUAL
        │   └── '&'  → TOKEN_AMPERSAND
        │
        ├── '|'
        │   ├── '||' → TOKEN_OR_OR
        │   ├── '|=' → TOKEN_PIPE_EQUAL
        │   └── '|'  → TOKEN_PIPE
        │
        ├── '^'
        │   ├── '^=' → TOKEN_CARET_EQUAL
        │   └── '^'  → TOKEN_CARET
        │
        ├── '~'
        │   └── TOKEN_TILDA
        │
        ├── '?'
        │   └── TOKEN_QUESTION
        │
        ├── '.'
        │   ├── '...' → TOKEN_RANGE
        │   └── '.'   → TOKEN_DOT
        │
        ├── '@'
        │   └── TOKEN_AT
        │
        ├── '{'
        │   └── TOKEN_LEFT_BRACE
        │
        ├── '}'
        │   └── TOKEN_RIGHT_BRACE
        │
        ├── '['
        │   └── TOKEN_LEFT_BRACKET
        │
        ├── ']'
        │   └── TOKEN_RIGHT_BRACKET
        │
        ├── '('
        │   └── TOKEN_LEFT_PAREN
        │
        ├── ')'
        │   └── TOKEN_RIGHT_PAREN
        │
        ├── ','
        │   └── TOKEN_COMMA
        │
        ├── ':'
        │   └── TOKEN_COLON
        │
        ├── ';'
        │   └── TOKEN_SEMICOLON
        │
        ├── '\''
        │   └── read_single_quoted_value()
        │       ├── Find closing '\''
        │       ├── Reject newline
        │       ├── Reject unterminated string
        │       └── TOKEN_STRING
        │
        ├── '"'
        │   └── read_string()
        │       ├── Find closing '"'
        │       ├── Process \n
        │       ├── Reject newline
        │       ├── Reject unterminated string
        │       └── TOKEN_STRING
        │
        ├── digit
        │   └── read_number()
        │       ├── Integer
        │       │   └── TOKEN_INTEGER
        │       │
        │       └── Decimal
        │           └── TOKEN_FLOAT
        │
        ├── identifier_start
        │   └── read_identifier()
        │       │
        │       ├── Reserved keywords
        │       │   ├── say
        │       │   ├── ask
        │       │   ├── var
        │       │   ├── if
        │       │   ├── elif
        │       │   ├── else
        │       │   ├── repeat
        │       │   └── end
        │       │
        │       ├── Boolean literals
        │       │   ├── true
        │       │   └── false
        │       │
        │       ├── Special values
        │       │   ├── NULL / null
        │       │   └── INF / inf
        │       │
        │       ├── Logical keywords
        │       │   └── logical_keyword_token_type()
        │       │
        │       ├── Identity keywords
        │       │   └── identity_keyword_token_type()
        │       │
        │       └── Otherwise
        │           └── TOKEN_IDENTIFIER
        │
        └── Unknown character
            └── Syntax Error
                └── Return NULL

After loop
│
├── Check lexer->in_block_comment
│   ├── Yes → Unterminated block comment → Return NULL
│   └── No  → Continue
│
├── Append TOKEN_EOF
│
├── Set token_count
│
└── return tokens
*/