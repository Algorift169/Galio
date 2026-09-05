/* SPDX-License-Identifier: AGPL-3.0-only */
/* AST declarations connect parser-produced nodes with later statement evaluation. */

#ifndef DRIFT_AST_H
#define DRIFT_AST_H

#include "drift/value.h"
#include "drift/array.h"
#include "drift/operator.h"

typedef struct {
    char *name;
    Value value;
    int is_declaration;
    int is_assignment;
    int is_array_element_assignment;
    int is_array_expression;
    int is_array_declared;
    int is_input_expression;
    char *input_prompt;
    char *input_target;
    char *expression_text;
    int has_expression;
    int has_assignment_operator;
    OperatorType assignment_operator;
    ArrayAccess array_access;
} VariableDeclarationSingle;

typedef struct {
    VariableDeclarationSingle *vars;
    size_t count;
} VariableDeclaration;

typedef struct {
    char *value;
    int is_variable_reference;
    int has_array_access;
    char *expression_text;
    int has_expression;
    ArrayAccess array_access;
} PrintStatement;

typedef struct {
    char *prompt;
    char *target_name;
    int has_prompt;
    int has_target;
} InputItem;

typedef struct {
    InputItem *items;
    size_t count;
} InputStatement;

typedef struct {
    char *counter_name;
    int declares_counter;
    int has_range;
    int has_step;
    int is_infinite;
    int is_exclusive_upper;
    int is_exclusive_lower;
    char *start_text;
    char *end_text;
    char *step_text;
    struct Statement *body;
    size_t body_count;
} RepeatStatement;

/* Stores the three deferred clauses and body of a basic for loop. */
typedef struct {
    char *init_text;
    int declares_counter;
    char *condition_text;
    char *increment_text;
    struct Statement *body;
    size_t body_count;
} ForStatement;

/* Stores a deferred condition and parsed body for a while loop. */
typedef struct {
    char *condition_text;
    struct Statement *body;
    size_t body_count;
} WhileStatement;

/* Stores the deferred source expression and body for an each loop. */
typedef struct {
    char *item_name;
    char *source_text;
    char *start_text;
    char *end_text;
    int is_range;
    struct Statement *body;
    size_t body_count;
} EachStatement;

/* Stores the deferred condition and body for an unless statement. */
typedef struct {
    char *condition_text;
    struct Statement *body;
    size_t body_count;
} UnlessStatement;

/* Stores one value expression and its body for a when case. */
typedef struct {
    char *value_text;
    struct Statement *body;
    size_t body_count;
} WhenCase;

/* Stores the subject, ordered cases, and optional else body for when. */
typedef struct {
    char *subject_text;
    WhenCase *cases;
    size_t case_count;
    struct Statement *else_body;
    size_t else_count;
} WhenStatement;

/* Stores a function name, parameter names, and the statements executed on call. */
typedef struct FunctionStatement {
    char *name;
    char **parameters;
    size_t parameter_count;
    struct Statement *body;
    size_t body_count;
} FunctionStatement;

/* Stores a function call name and deferred argument expressions. */
typedef struct {
    char *name;
    char **arguments;
    size_t argument_count;
} FunctionCallStatement;

/* Stores the deferred expression returned by a function. */
typedef struct {
    char *expression_text;
} ReturnStatement;

#endif
