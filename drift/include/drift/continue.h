/* SPDX-License-Identifier: AGPL-3.0-only */
/* Continue execution is kept separate from the general statement interpreter. */

#ifndef DRIFT_CONTINUE_H
#define DRIFT_CONTINUE_H

/* Produces the internal result consumed by the nearest enclosing loop. */
int interpreter_execute_continue(void);

#endif