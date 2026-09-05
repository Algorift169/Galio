/* SPDX-License-Identifier: AGPL-3.0-only */
/* Statement nodes preserve parsed control-flow structure until the interpreter executes it. */

#ifndef DRIFT_STATEMENT_H
#define DRIFT_STATEMENT_H

#include "drift/ast.h"

struct Statement;

// PrintStatement represents a print statement in the AST.
// But it also represents a variable declaration statement, since the
// variable declaration statement is a special case of the print statement.
// SO the print statement is used to represent both print statements and variable
// declaration statements.
// The is_variable_reference field indicates whether the value is 
// a variable reference or a literal value. If it is a variable reference
typedef struct {
    char *condition_text;
    struct Statement *body;
    size_t body_count;
} IfBranch;

typedef struct {
    IfBranch *branches;
    size_t branch_count;
    struct Statement *else_body;
    size_t else_count;
} IfStatement;

typedef enum {
    STATEMENT_PRINT,
    STATEMENT_VARIABLE_DECLARATION,
    STATEMENT_INPUT,
    STATEMENT_IF,
    STATEMENT_REPEAT,
    STATEMENT_FOR,
    STATEMENT_WHILE,
    STATEMENT_BREAK,
    STATEMENT_CONTINUE,
    STATEMENT_EACH,
    STATEMENT_UNLESS,
    STATEMENT_WHEN,
    STATEMENT_FUNCTION,
    STATEMENT_FUNCTION_CALL,
    STATEMENT_RETURN
} StatementType;

typedef struct Statement {
    StatementType type;
    union {
        PrintStatement print_statement;
        VariableDeclaration variable_declaration;
        InputStatement input_statement;
        IfStatement if_statement;
        RepeatStatement repeat_statement;
        ForStatement for_statement;
        WhileStatement while_statement;
        EachStatement each_statement; // Added EachStatement to the union to represent each statements in the AST.
        UnlessStatement unless_statement;
        WhenStatement when_statement;
        FunctionStatement function_statement;
        FunctionCallStatement function_call_statement;
        ReturnStatement return_statement;
    } as;
} Statement;

#endif
