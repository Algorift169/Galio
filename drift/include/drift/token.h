/* SPDX-License-Identifier: AGPL-3.0-only */
/* Tokens carry lexical spans and values across the lexer-parser boundary; callers release them explicitly. */

#ifndef DRIFT_TOKEN_H
#define DRIFT_TOKEN_H

#include <stddef.h>

/*
 The TokenType enumeration defines the different types of tokens 
 that can be recognized by the lexer. Each token type corresponds 
 to a specific kind of lexical element in the source code, 
such as keywords, operators, literals, and punctuation. 
The enumeration provides a comprehensive set of token types to facilitate
the parsing and interpretation of the source code in the Drift programming language.
*/
typedef enum {
    TOKEN_SAY,
    TOKEN_ASK,
    TOKEN_VAR,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_REPEAT,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_UNLESS,
    TOKEN_WHEN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_EACH,
    TOKEN_FUN,
    TOKEN_RETURN,
    TOKEN_END,
    TOKEN_AT,
    TOKEN_IN,
    TOKEN_IS,
    TOKEN_IDENTIFIER,
    TOKEN_EQUAL,
    TOKEN_QUESTION,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_GREATER,
    TOKEN_LESS,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_BANG,
    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDA,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_RIGHT,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL,
    TOKEN_AMPERSAND_EQUAL,
    TOKEN_PIPE_EQUAL,
    TOKEN_CARET_EQUAL,
    TOKEN_SHIFT_LEFT_EQUAL,
    TOKEN_SHIFT_RIGHT_EQUAL,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_RANGE,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_INFINITY,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_NEWLINE,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

// The Token structure represents a single token in the source code,
// containing its type and associated value. The value is a string that
// holds the actual text of the token, which can be used for 
// further processing or analysis during parsing and interpretation.
typedef struct {
    TokenType type;
    char *value;
    size_t indentation;
} Token;

char *drift_duplicate_string(const char *value);
/* Releases each token string and then the token array that owns those strings. */
void token_free_array(Token *tokens, size_t count);

#endif
