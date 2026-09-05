/* SPDX-License-Identifier: AGPL-3.0-only */
/* Logical keyword declarations map language words to the same operator semantics as symbols. */

#ifndef DRIFT_LOGICAL_KEYWORDS_H
#define DRIFT_LOGICAL_KEYWORDS_H

#include "drift/token.h"

/* Maps a logical keyword spelling to its token or returns unknown. */
TokenType logical_keyword_token_type(const char *value);

#endif