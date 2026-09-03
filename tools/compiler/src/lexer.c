#include "lexer.h"

typedef struct { const char *name; gc_token_kind_t kind; } keyword_t;
static const keyword_t keywords[] = {
    {"int", TOK_INT}, {"char", TOK_CHAR_KW}, {"void", TOK_VOID}, {"return", TOK_RETURN},
    {"if", TOK_IF}, {"else", TOK_ELSE}, {"while", TOK_WHILE}, {"for", TOK_FOR},
    {"do", TOK_DO}, {"break", TOK_BREAK}, {"continue", TOK_CONTINUE}, {"extern", TOK_EXTERN},
    {"static", TOK_STATIC}, {"const", TOK_CONST}, {"sizeof", TOK_SIZEOF}, {"typedef", TOK_TYPEDEF},
    {"struct", TOK_STRUCT}, {"union", TOK_UNION}, {"enum", TOK_ENUM}, {0, TOK_EOF}
};

static u8 is_digit(char c) { return c >= '0' && c <= '9'; }
static u8 is_hex(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static u8 is_ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static u8 is_ident_char(char c) { return is_ident_start(c) || is_digit(c); }

static void advance(gc_lexer_t *lex) {
    if (*lex->cursor == '\n') { lex->line++; lex->column = 1; }
    else lex->column++;
    lex->cursor++;
}

static gc_token_kind_t keyword_kind(const char *start, u32 length) {
    u32 i, n;
    for (i = 0; keywords[i].name; i++) {
        for (n = 0; keywords[i].name[n] && n < length && keywords[i].name[n] == start[n]; n++) {}
        if (n == length && keywords[i].name[n] == 0) return keywords[i].kind;
    }
    return TOK_IDENT;
}

static i32 read_escape(gc_lexer_t *lex) {
    char c = *lex->cursor;
    advance(lex);
    if (c == 'n') return '\n';
    if (c == 't') return '\t';
    if (c == 'r') return '\r';
    if (c == '\\') return '\\';
    if (c == '"') return '"';
    if (c == '\'') return '\'';
    if (c == '0') return 0;
    if (c == 'x') {
        i32 value = 0; u32 count = 0;
        while (count < 2 && is_hex(*lex->cursor)) {
            char digit = *lex->cursor++;
            value = value * 16 + (is_digit(digit) ? digit - '0' :
                (digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : digit - 'A' + 10));
            count++;
        }
        return value;
    }
    return (u8)c;
}

void gc_lexer_init(gc_lexer_t *lex, const char *source) {
    lex->source = source;
    lex->cursor = source;
    lex->line = 1;
    lex->column = 1;
}

int gc_lex_next(gc_lexer_t *lex, gc_token_t *tok) {
    const char *start;
    u32 value = 0;
    while (*lex->cursor) {
        if (*lex->cursor == ' ' || *lex->cursor == '\t' || *lex->cursor == '\r' || *lex->cursor == '\n') { advance(lex); continue; }
        if (lex->cursor[0] == '/' && lex->cursor[1] == '/') { while (*lex->cursor && *lex->cursor != '\n') advance(lex); continue; }
        if (lex->cursor[0] == '/' && lex->cursor[1] == '*') {
            advance(lex); advance(lex);
            while (*lex->cursor && !(lex->cursor[0] == '*' && lex->cursor[1] == '/')) advance(lex);
            if (*lex->cursor) { advance(lex); advance(lex); }
            continue;
        }
        break;
    }
    start = lex->cursor;
    tok->start = start;
    tok->value = 0;
    if (!*start) { tok->kind = TOK_EOF; tok->length = 0; return 0; }
    if (is_ident_start(*lex->cursor)) {
        while (is_ident_char(*lex->cursor)) advance(lex);
        tok->kind = keyword_kind(start, (u32)(lex->cursor - start));
    } else if (is_digit(*lex->cursor)) {
        if (lex->cursor[0] == '0' && (lex->cursor[1] == 'x' || lex->cursor[1] == 'X')) {
            advance(lex); advance(lex);
            while (is_hex(*lex->cursor)) { char c = *lex->cursor; advance(lex); value = value * 16 + (is_digit(c) ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : c - 'A' + 10)); }
        } else if (lex->cursor[0] == '0' && (lex->cursor[1] == 'b' || lex->cursor[1] == 'B')) {
            advance(lex); advance(lex);
            while (*lex->cursor == '0' || *lex->cursor == '1') { value = value * 2 + (u32)(*lex->cursor - '0'); advance(lex); }
        } else {
            while (is_digit(*lex->cursor)) { value = value * 10 + (u32)(*lex->cursor - '0'); advance(lex); }
        }
        tok->kind = TOK_NUMBER; tok->value = (i32)value;
    } else if (*lex->cursor == '"') {
        advance(lex);
        while (*lex->cursor && *lex->cursor != '"') { if (*lex->cursor == '\\') { advance(lex); if (*lex->cursor) (void)read_escape(lex); } else advance(lex); }
        if (*lex->cursor) advance(lex);
        tok->kind = TOK_STRING;
    } else if (*lex->cursor == '\'') {
        advance(lex);
        if (*lex->cursor == '\\') { advance(lex); value = (u32)read_escape(lex); }
        else { value = (u8)*lex->cursor; if (*lex->cursor) advance(lex); }
        if (*lex->cursor == '\'') advance(lex);
        tok->kind = TOK_CHAR; tok->value = (i32)value;
    } else {
        static const char *ops[] = {"++", "--", "+=", "-=", "*=", "/=", "%=", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "->", 0};
        static const gc_token_kind_t kinds[] = {TOK_INC, TOK_DEC, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_STAR_ASSIGN, TOK_SLASH_ASSIGN, TOK_PERCENT_ASSIGN, TOK_EQ, TOK_NE, TOK_LE, TOK_GE, TOK_LOGICAL_AND, TOK_LOGICAL_OR, TOK_SHL, TOK_SHR, TOK_ARROW};
        u32 i; tok->kind = TOK_EOF;
        for (i = 0; ops[i]; i++) if (lex->cursor[0] == ops[i][0] && lex->cursor[1] == ops[i][1]) { advance(lex); advance(lex); tok->kind = kinds[i]; break; }
        if (tok->kind == TOK_EOF) { char c = *lex->cursor; advance(lex); switch (c) {
            case '+': tok->kind = TOK_PLUS; break; case '-': tok->kind = TOK_MINUS; break; case '*': tok->kind = TOK_STAR; break; case '/': tok->kind = TOK_SLASH; break; case '%': tok->kind = TOK_PERCENT; break; case '=': tok->kind = TOK_ASSIGN; break; case '<': tok->kind = TOK_LT; break; case '>': tok->kind = TOK_GT; break; case '&': tok->kind = TOK_BIT_AND; break; case '|': tok->kind = TOK_BIT_OR; break; case '^': tok->kind = TOK_BIT_XOR; break; case '~': tok->kind = TOK_BIT_NOT; break; case '!': tok->kind = TOK_LOGICAL_NOT; break; case '.': tok->kind = TOK_DOT; break; case '(': tok->kind = TOK_LPAREN; break; case ')': tok->kind = TOK_RPAREN; break; case '{': tok->kind = TOK_LBRACE; break; case '}': tok->kind = TOK_RBRACE; break; case '[': tok->kind = TOK_LBRACKET; break; case ']': tok->kind = TOK_RBRACKET; break; case ';': tok->kind = TOK_SEMICOLON; break; case ',': tok->kind = TOK_COMMA; break; case ':': tok->kind = TOK_COLON; break; case '?': tok->kind = TOK_QUESTION; break; case '#': tok->kind = TOK_HASH; break; default: return -1; }
        }
    }
    tok->length = (u32)(lex->cursor - start);
    return 0;
}
