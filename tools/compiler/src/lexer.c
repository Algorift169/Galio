#include "lexer.h"

static int gc_digit(char c) { return c >= '0' && c <= '9'; }
static int gc_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

int gc_lex_next(const char **cursor, gc_token_t *token) {
    const char *p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '/' && p[1] == '/') { while (*p && *p != '\n') p++; return gc_lex_next(&p, token); }
    if (*p == '/' && p[1] == '*') { p += 2; while (*p && !(p[0] == '*' && p[1] == '/')) p++; if (*p) p += 2; return gc_lex_next(&p, token); }
    token->start = p; token->value = 0;
    if (!*p) { token->kind = GC_T_EOF; token->length = 0; *cursor = p; return 0; }
    if (gc_alpha(*p)) { do p++; while (gc_alpha(*p) || gc_digit(*p)); token->kind = GC_T_IDENT; }
    else if (gc_digit(*p)) { while (gc_digit(*p)) { token->value = token->value * 10 + (*p++ - '0'); } token->kind = GC_T_NUMBER; }
    else if (*p == '"') { p++; token->start = p; while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; } token->length = (u32)(p - token->start); if (*p) p++; token->kind = GC_T_STRING; *cursor = p; return 0; }
    else { p++; token->kind = GC_T_SYMBOL; }
    token->length = (u32)(p - token->start); *cursor = p; return 0;
}
