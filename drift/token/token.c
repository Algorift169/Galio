/* SPDX-License-Identifier: AGPL-3.0-only */
/* Token construction transfers text ownership 
into token storage and destruction releases it exactly 
once. */

#include <stdlib.h>
#include <string.h>

#include "drift/token.h"

/* Allocates and copies token text so token storage owns its input independently. */
char *drift_duplicate_string(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1U); // Reserve space for text and its terminator.
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

/* Frees token text first, then the containing array that owns the token records. */
void token_free_array(Token *tokens, size_t count)
{
    size_t i;

    if (tokens == NULL) {
        return;
    }

    for (i = 0; i < count; ++i) { 
        // Each token owns only its value string here.
        // The token array itself is freed after all values are released.
        free(tokens[i].value);
    }

    free(tokens);
}
