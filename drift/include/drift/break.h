/* SPDX-License-Identifier: AGPL-3.0-only */
/* Break execution is kept separate from the general statement interpreter. */

#ifndef DRIFT_BREAK_H
#define DRIFT_BREAK_H

/* Produces the internal result consumed by the nearest enclosing loop. */
int interpreter_execute_break(void);

#endif