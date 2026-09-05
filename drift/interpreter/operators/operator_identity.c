/* SPDX-License-Identifier: AGPL-3.0-only */
/* Identity compares the language-level value identity rules rather than C pointer addresses. */

#include <stddef.h>

#include "drift/operator.h"

/* Compares type and shared identity markers; null operands cannot be identical values. */
Value operator_apply_identity(const Value *left, const Value *right)
{
    /* Identity requires both the same runtime type and the same identity
       marker, rather than merely equal contents. */
    if (left == NULL || right == NULL) {
        return value_create_boolean(0);
    }

    return value_create_boolean(left->type == right->type &&
                                left->identity != NULL &&
                                left->identity == right->identity);
}