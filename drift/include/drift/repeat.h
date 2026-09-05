/* SPDX-License-Identifier: AGPL-3.0-only */
/* Repeat runtime execution is kept separate from the general statement interpreter. */

#ifndef DRIFT_REPEAT_H
#define DRIFT_REPEAT_H

#include "drift/environment.h"
#include "drift/ast.h"

/* Executes one parsed repeat statement against the supplied environment. */
int interpreter_execute_repeat(RepeatStatement *repeat_statement, Environment *environment);

#endif
