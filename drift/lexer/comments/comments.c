/* SPDX-License-Identifier: AGPL-3.0-only */
/* Comment scanning advances through line and block forms while preserving lexer position at the next token. */

#include <stdio.h>
#include <string.h>
#include "drift/comments.h"

/* Scans source while skipping quoted text and line comments before checking block balance. */
int is_block_comment_open(const char *source)
{
    if (source == NULL) {
        return 0;
    }

    int in_block = 0;
    size_t i = 0;
    while (source[i] != '\0') { // Advance one lexical construct at a time.
        if (!in_block && (source[i] == '\'' || source[i] == '"')) {
            char quote = source[i];
            i++;
            while (source[i] != '\0' && source[i] != quote) {
                if (source[i] == '\\' && source[i + 1] != '\0') {
                    i += 2;
                } else {
                    i++;
                }
            }
            if (source[i] == quote) {
                i++;
            }
            continue;
        }

        if (!in_block && source[i] == '/' && source[i + 1] == '/') {
            i += 2;
            while (source[i] != '\0' && source[i] != '\n') {
                i++;
            }
            continue;
        }

        if (!in_block && source[i] == '/' && source[i + 1] == '*') {
            in_block = 1;
            i += 2;
            continue;
        }

        if (in_block && source[i] == '*' && source[i + 1] == '/') {
            in_block = 0;
            i += 2;
            continue;
        }

        i++;
    }

    return in_block;
}

void comments_reset_state(void)
{
}

/* Consumes the comment beginning at the current lexer position and updates state for blocks. */
int lexer_skip_comments(Lexer *lexer)
{
    if (lexer == NULL || lexer->source == NULL || lexer->index >= lexer->length) {
        return 0;
    }

    // Handle case where we're already inside a block comment
    if (lexer->in_block_comment) { // Resume an earlier block before looking for new syntax.
        // Skip all characters until we find */
        while (lexer->index < lexer->length) {
            if (lexer->source[lexer->index] == '*' &&
                lexer->index + 1 < lexer->length &&
                lexer->source[lexer->index + 1] == '/') {
                // Found end of block comment
                lexer->in_block_comment = 0;
                lexer->in_block_comment_code = 0;
                lexer->index += 2;
                return 1;
            }
            lexer->index++;
        }
        // Reached EOF while in block comment (unterminated comment)
        return 1;
    }

    // Check if starting a new comment
    if (lexer->source[lexer->index] == '/' && lexer->index + 1 < lexer->length) {
        char next = lexer->source[lexer->index + 1];

        // Single-line comment
        if (next == '/') {
            lexer->index += 2;
            while (lexer->index < lexer->length && lexer->source[lexer->index] != '\n') {
                lexer->index++;
            }
            return 1;
        }

        // Block comment
        if (next == '*') {
            lexer->in_block_comment = 1;
            lexer->in_block_comment_code = 0;
            lexer->index += 2;
            
            // Skip all characters until we find */
            while (lexer->index < lexer->length) {
                if (lexer->source[lexer->index] == '*' &&
                    lexer->index + 1 < lexer->length &&
                    lexer->source[lexer->index + 1] == '/') {
                    // Found end of block comment
                    lexer->in_block_comment = 0;
                    lexer->in_block_comment_code = 0;
                    lexer->index += 2;
                    return 1;
                }
                lexer->index++;
            }
            // Reached EOF while in block comment (unterminated comment)
            return 1;
        }
    }

    return 0;
}
