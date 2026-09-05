/* SPDX-License-Identifier: AGPL-3.0-only */
/* When runtime execution is kept separate from the general interpreter. */

#ifndef DRIFT_WHEN_H
#define DRIFT_WHEN_H

#include "drift/ast.h"
#include "drift/environment.h"

/* Executes one parsed when statement. */
int interpreter_execute_when(WhenStatement *statement, Environment *environment);

#endif