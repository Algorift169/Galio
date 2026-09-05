/* SPDX-License-Identifier: AGPL-3.0-only */
/* Continue execution signals the nearest enclosing loop to start its next pass. */

#include "drift/continue.h"
#include "drift/control_flow.h"

int interpreter_execute_continue(void)
{
    return DRIFT_EXECUTION_CONTINUE;
}