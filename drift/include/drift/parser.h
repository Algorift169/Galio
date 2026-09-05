/* SPDX-License-Identifier: AGPL-3.0-only */
/* Parser declarations describe cursor movement 
and the invariant that input is consumed once. */

#ifndef DRIFT_PARSER_H
#define DRIFT_PARSER_H

#include <stddef.h>

#include "drift/ast.h"
#include "drift/statement.h"
#include "drift/token.h"

// Parser structure to hold the state of the parser.
typedef struct Parser {
    Token *tokens;
    size_t count;
    size_t index;
} Parser;
 
/*
 The parser_create function initializes a Parser structure with the given 
  tokens and count. It sets the index to 0, indicating that parsing will
    start from the beginning of the token array.
*/
Parser parser_create(Token *tokens, size_t count);
Statement parser_parse(Parser *parser);
int parse_if_statement(Parser *parser, Statement *statement);
int parse_repeat_statement(Parser *parser, Statement *statement);
int parse_for_statement(Parser *parser, Statement *statement);
int parse_while_statement(Parser *parser, Statement *statement);
// Parses an each statement, including its loop variable, source expression, and body statements.
int parse_each_statement(Parser *parser, Statement *statement);
int parse_unless_statement(Parser *parser, Statement *statement);
int parse_when_statement(Parser *parser, Statement *statement);
int parse_function_statement(Parser *parser, Statement *statement);
int parse_return_statement(Parser *parser, Statement *statement);
int parse_function_call_statement(Parser *parser, Statement *statement);
void if_statement_free(IfStatement *statement);
void repeat_statement_free(RepeatStatement *statement);
void for_statement_free(ForStatement *statement);
void while_statement_free(WhileStatement *statement);
// Frees the resources associated with an EachStatement, including its body statements and
// any dynamically allocated strings. This function should be called when the EachStatement
// is no longer needed to avoid memory leaks.
void each_statement_free(EachStatement *statement);
void unless_statement_free(UnlessStatement *statement);
void when_statement_free(WhenStatement *statement);
void function_statement_free(FunctionStatement *statement);
void function_call_statement_free(FunctionCallStatement *statement);
void return_statement_free(ReturnStatement *statement);
void print_statement_free(PrintStatement *statement);
void variable_declaration_free(VariableDeclaration *declaration);

#endif
