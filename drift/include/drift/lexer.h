/* SPDX-License-Identifier: AGPL-3.0-only */
/* Lexer contracts expose tokenization state while retaining ownership of source-backed text. */

#ifndef DRIFT_LEXER_H
#define DRIFT_LEXER_H

#include <stddef.h>

#include "drift/token.h"

typedef struct {
    const char *source;
    size_t index;
    size_t length;
    int in_block_comment;
    int in_block_comment_code;
    int at_line_start;
    size_t line_indentation;
} Lexer;

/* Copies source text and initializes lexer position and comment state. */
Lexer lexer_create(const char *source);
Token *lexer_scan_all(Lexer *lexer, size_t *token_count);

#endif
