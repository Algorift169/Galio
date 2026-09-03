#ifndef GC_LEXER_H
#define GC_LEXER_H

#include "common.h"

typedef enum {
	TOK_EOF, TOK_NUMBER, TOK_STRING, TOK_CHAR, TOK_IDENT,
	TOK_INT, TOK_CHAR_KW, TOK_VOID, TOK_RETURN, TOK_IF, TOK_ELSE,
	TOK_WHILE, TOK_FOR, TOK_DO, TOK_BREAK, TOK_CONTINUE, TOK_EXTERN,
	TOK_STATIC, TOK_CONST, TOK_SIZEOF, TOK_TYPEDEF, TOK_STRUCT, TOK_UNION, TOK_ENUM,
	TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_INC, TOK_DEC,
	TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN, TOK_SLASH_ASSIGN,
	TOK_PERCENT_ASSIGN, TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
	TOK_LOGICAL_AND, TOK_LOGICAL_OR, TOK_LOGICAL_NOT, TOK_BIT_AND, TOK_BIT_OR,
	TOK_BIT_XOR, TOK_BIT_NOT, TOK_SHL, TOK_SHR, TOK_ARROW, TOK_DOT, TOK_ADDRESS, TOK_DEREF,
	TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
	TOK_SEMICOLON, TOK_COMMA, TOK_COLON, TOK_QUESTION, TOK_HASH
} gc_token_kind_t;
typedef struct { gc_token_kind_t kind; const char *start; u32 length; i32 value; } gc_token_t;
typedef struct { const char *source; const char *cursor; u32 line; u32 column; } gc_lexer_t;
void gc_lexer_init(gc_lexer_t *lex, const char *source);
int gc_lex_next(gc_lexer_t *lex, gc_token_t *token);

#endif
