/* SPDX-License-Identifier: AGPL-3.0-only */
/* Function APIs connect parsed definitions and calls with 
runtime environments. */

#ifndef DRIFT_FUNCTION_H
#define DRIFT_FUNCTION_H

#include "drift/environment.h"
#include "drift/statement.h"

#define DRIFT_EXECUTION_RETURN 4

int interpreter_execute_function(FunctionStatement *statement, Environment *environment);
int interpreter_execute_function_call(FunctionCallStatement *statement, Environment *environment);
int interpreter_execute_return(ReturnStatement *statement, Environment *environment);

#endif
