/* SPDX-License-Identifier: AGPL-3.0-only */
/* Unless runtime execution is kept separate from the general interpreter. */

#ifndef DRIFT_UNLESS_H
#define DRIFT_UNLESS_H

#include "drift/ast.h"
#include "drift/environment.h"

/* Executes one parsed unless statement. */
int interpreter_execute_unless(UnlessStatement *statement, Environment *environment);

#endif