/* SPDX-License-Identifier: AGPL-3.0-only */
/* Break execution signals the nearest enclosing loop to stop. */

#include "drift/break.h"
#include "drift/control_flow.h"

int interpreter_execute_break(void)
{
    return DRIFT_EXECUTION_BREAK;
}