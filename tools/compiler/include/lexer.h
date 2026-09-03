#ifndef GC_LEXER_H
#define GC_LEXER_H

#include "common.h"

typedef enum { GC_T_EOF, GC_T_IDENT, GC_T_NUMBER, GC_T_STRING, GC_T_SYMBOL } gc_token_kind_t;
typedef struct { gc_token_kind_t kind; const char *start; u32 length; i32 value; } gc_token_t;
int gc_lex_next(const char **cursor, gc_token_t *token);

#endif
