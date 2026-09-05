/* SPDX-License-Identifier: AGPL-3.0-only */
/* Comment APIs separate ordinary source comments from tokens visible to the lexer. */

#ifndef DRIFT_COMMENTS_H
#define DRIFT_COMMENTS_H

#include "drift/lexer.h"

/* Advances the lexer over one line or block comment when one starts at index. */
int lexer_skip_comments(Lexer *lexer);
/* Scans source while ignoring quoted text and reports an unclosed block. */
int is_block_comment_open(const char *source);
/* Resets comment-module state retained between lexer operations. */
void comments_reset_state(void);

#endif
