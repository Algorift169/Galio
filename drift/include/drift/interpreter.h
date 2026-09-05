/* SPDX-License-Identifier: AGPL-3.0-only */
/* Interpreter entry points consume parsed statements in source order and report runtime errors. */

#ifndef DRIFT_INTERPRETER_H
#define DRIFT_INTERPRETER_H

#include "drift/environment.h"
#include "drift/break.h"
#include "drift/continue.h"
#include "drift/control_flow.h"
#include "drift/each.h"
#include "drift/unless.h"
#include "drift/when.h"
#include "drift/function.h"
#include "drift/parser.h"
#include "drift/statement.h"

/* Evaluates one parsed statement against the supplied runtime environment. */
int interpreter_execute(Statement statement, Environment *environment);

/* Evaluates expression text using the interpreter's existing expression rules. */
Value interpreter_evaluate_expression(Environment *environment, const char *expression, int *ok);

/* Executes the parsed basic for and while loop forms. */
int interpreter_execute_for(ForStatement *for_statement, Environment *environment);
// Executes the parsed while loop form. The condition is evaluated before 
//every iteration, and the body is executed if the condition is truthy. If a nested statement returns DRIFT_EXECUTION_BREAK or DRIFT_EXECUTION_CONTINUE, that result is propagated to the caller so the loop can handle it appropriately.
int interpreter_execute_while(WhileStatement *while_statement, Environment *environment);

#endif
