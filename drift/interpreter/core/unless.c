/* SPDX-License-Identifier: AGPL-3.0-only */
/* Unless execution runs its body when the deferred condition is false. 

unless is a conditional execution form that evaluates a deferred condition and runs its body 
only if the condition evaluates to false. If the condition is true, the body is skipped. However, if
the condition is false, the body is executed. The unless statement is useful for cases where 
you want to execute a block of code only when a certain condition is not met, providing a clear and 
concise way to express negative conditions in your code. It is often used as an alternative to 
if statements when you want to emphasize the negative case, making the code more readable and 
expressive. The unless statement can be used in various programming scenarios, such as input validation,
    error handling, and conditional execution of code blocks based on specific criteria. It allows 
    developers to write cleaner and more maintainable code by reducing the need for nested if 
statements and improving the overall flow of the program. By using unless, you can clearly convey
    the intent of your code, making it easier for others to understand the logic and purpose of the 
    conditional execution. Overall, the unless statement is a valuable addition to the programming 
    language, providing a more expressive and readable way to handle negative conditions in your code.
*/

// fun fact: the unless statement is just an if statement with a negated condition.
#include <stdio.h>

#include "drift/control_flow.h"
#include "drift/interpreter.h"
#include "drift/operator.h"
#include "drift/unless.h"

int interpreter_execute_unless(UnlessStatement *statement, Environment *environment)
{
    int ok = 0;
    Value condition;
    Value inverted;

    if (statement == NULL || environment == NULL || statement->condition_text == NULL) {
        fprintf(stderr, "Runtime Error: Invalid unless statement.\n");
        return DRIFT_EXECUTION_ERROR;
    }

    // condition is evaluated first, and if it is true, the body is skipped. If it is false, the body
    // is executed. This is the opposite of an if statement, which executes the body when the condition
    // is true. The unless statement is useful for cases where you want to execute a block of code
    // only when a certain condition is not met, providing a clear and concise way to express
    // negative conditions in your code. It is often used as an alternative to if statements when
    // you want to emphasize the negative case, making the code more readable and expressive.
    condition = interpreter_evaluate_expression(environment, statement->condition_text, &ok);
    if (!ok) {
        value_free(&condition);
        fprintf(stderr, "Runtime Error: Failed to evaluate unless condition '%s'.\n", statement->condition_text);
        return DRIFT_EXECUTION_ERROR;
    }
    inverted = operator_apply(OPERATOR_NOT, &condition, NULL);
    value_free(&condition);
    if (!inverted.boolean_value) {
        value_free(&inverted);
        return DRIFT_EXECUTION_OK;
    }
    value_free(&inverted);

    for (size_t i = 0; i < statement->body_count; ++i) {
        int result = interpreter_execute(statement->body[i], environment);
        if (result != DRIFT_EXECUTION_OK) {
            return result;
        }
    }
    return DRIFT_EXECUTION_OK;
}
