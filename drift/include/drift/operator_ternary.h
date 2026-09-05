/* SPDX-License-Identifier: AGPL-3.0-only */
/* Ternary evaluation selects one result after applying the language truthiness rules. */

#ifndef DRIFT_OPERATOR_TERNARY_H
#define DRIFT_OPERATOR_TERNARY_H

#include "drift/value.h"

/* Selects and copies exactly one branch value according to condition truth. */
Value operator_apply_ternary(const Value *condition, const Value *true_value, const Value *false_value);

#endif