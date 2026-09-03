#include "parser.h"
#include "lexer.h"

int gc_parse_source(const char *source) {
    const char *cursor = source; gc_token_t token;
    while (gc_lex_next(&cursor, &token) == 0 && token.kind != GC_T_EOF) { }
    return source ? 0 : -4;
}
