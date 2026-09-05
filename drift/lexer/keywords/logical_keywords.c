/* SPDX-License-Identifier: AGPL-3.0-only */
/* Logical words share token meanings with symbolic operators so 
both forms reach one evaluator path. */

#include <string.h>

#include "drift/logical_keywords.h"

/* Converts word-form logical operators into the same tokens used by 
symbols. Supporting both forms allows the lexer to produce a single token 
stream for the parser and interpreter. 
*/
TokenType logical_keyword_token_type(const char *value)
{
    if (value == NULL) {
        return TOKEN_UNKNOWN;
    }

    if (strcmp(value, "and") == 0) {
        return TOKEN_AND_AND;
    }

    if (strcmp(value, "or") == 0) {
        return TOKEN_OR_OR;
    }

    if (strcmp(value, "not") == 0) {
        return TOKEN_BANG;
    }

    return TOKEN_UNKNOWN;
}