/* SPDX-License-Identifier: AGPL-3.0-only */
/* Identity keyword declarations keep lexical spellings centralized for the parser. */

#ifndef DRIFT_IDENTITY_KEYWORDS_H
#define DRIFT_IDENTITY_KEYWORDS_H

#include "drift/token.h"

/* Maps the identity keyword spelling to its token or returns unknown. */
TokenType identity_keyword_token_type(const char *value);

#endif