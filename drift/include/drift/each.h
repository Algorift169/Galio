/* SPDX-License-Identifier: AGPL-3.0-only */
/* Each runtime execution is kept separate from the general statement interpreter. */

#ifndef DRIFT_EACH_H
#define DRIFT_EACH_H

#include "drift/ast.h"
#include "drift/environment.h"

/* Executes one parsed each loop against the supplied environment. */
int interpreter_execute_each(EachStatement *each_statement, Environment *environment);

#endif