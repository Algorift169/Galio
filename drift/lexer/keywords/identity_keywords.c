/* SPDX-License-Identifier: AGPL-3.0-only */
/* Identity words are recognized as operator tokens before the parser applies precedence. */

#include <string.h>

#include "drift/identity_keywords.h"

/* Converts the identity keyword into its parser token when the spelling matches. */
TokenType identity_keyword_token_type(const char *value)
{
    if (value != NULL && strcmp(value, "is") == 0) {
        return TOKEN_IS;
    }

    return TOKEN_UNKNOWN;
}