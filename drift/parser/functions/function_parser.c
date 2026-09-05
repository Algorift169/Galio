/* SPDX-License-Identifier: AGPL-3.0-only */
/* Function parsing collects a named parameter list and an
explicitly terminated body. 
The body is parsed into a list of statements, which may include
function calls and return statements. The return statement defers
expression evaluation until the function is executed. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/parser.h"

// Forward declarations for helper functions
static Token *peek(Parser *parser)
{
    /* Inspect the current token without consuming it. */
    return parser != NULL && parser->index < parser->count ? &parser->tokens[parser->index] : NULL;
}

static Token *advance(Parser *parser)
{
    /* Consume the current token and move to the next grammar item. */
    return peek(parser) == NULL ? NULL : &parser->tokens[parser->index++];
}

// Helper function to append a statement to the body list and 
// grow the list if necessary. The body list is dynamically allocated and resized
// as needed to accommodate new statements. The function ensures that 
// the order of statements is preserved while appending new statements.
static void append_statement(Statement **body, size_t *count, size_t *capacity, Statement statement)
{
    /* Grow the body list geometrically while preserving source order. */
    if (*count == *capacity) {
        size_t next = *capacity == 0U ? 4U : *capacity * 2U;
        Statement *items = (Statement *)realloc(*body, next * sizeof(Statement));
        if (items == NULL) return;
        *body = items;
        *capacity = next;
    }
    (*body)[(*count)++] = statement;
}

// Helper function to collect an expression from the parser until a
// specified stop token is encountered. The function builds a string 
// representation of the expression by concatenating token values,
// separated by spaces. The resulting string is dynamically allocated
// and must be freed by the caller. If no tokens are collected, an empty
// string is returned. If memory allocation fails, NULL is returned.
static char *collect_expression(Parser *parser, TokenType stop)
{
    /* Rebuild a return or call argument expression until its delimiter. */
    char *text = NULL;
    size_t length = 0;
    while (peek(parser) != NULL && peek(parser)->type != stop &&
           peek(parser)->type != TOKEN_NEWLINE && peek(parser)->type != TOKEN_EOF) {
        Token *token = advance(parser);
        size_t part = token->value == NULL ? 0U : strlen(token->value);
        char *next = (char *)realloc(text, length + part + 2U);
        if (next == NULL) {
            free(text);
            return NULL;
        }
        text = next;
        if (length > 0U) text[length++] = ' ';
        if (part > 0U) memcpy(text + length, token->value, part);
        length += part;
        text[length] = '\0';
    }
    if (text == NULL) text = drift_duplicate_string("");
    return text;
}

// Helper function to free the memory allocated for a function statement, 
// including its name, parameters, and body. The function recursively frees
// any nested function statements, function call statements, and return statements
// within the body. After freeing all associated memory, the function resets
// the fields of the FunctionStatement structure to zero to prevent dangling 
// pointers and ensure that the structure is in a clean state.
int parse_return_statement(Parser *parser, Statement *statement)
{
    /* Parse `return expression` and defer expression evaluation until the call. */
    ReturnStatement result;
    memset(&result, 0, sizeof(result));
    advance(parser);
    // Collect the expression text until the end of the line or EOF, and 
    // store it in the ReturnStatement structure. If a newline token is
    // encountered after the expression, it is consumed to prepare for the 
    // next statement. The function returns 1 if the expression was successfully
    // collected, or 0 if there was an error (e.g., memory allocation failure
    // or unexpected token).
    result.expression_text = collect_expression(parser, TOKEN_EOF);
    if (peek(parser) != NULL && peek(parser)->type == TOKEN_NEWLINE) advance(parser);
    statement->type = STATEMENT_RETURN;
    statement->as.return_statement = result;
    return result.expression_text != NULL;
}


// Helper function to free the memory allocated for a function call statement,
// including its name and arguments. The function iterates through the
// list of arguments, freeing each one individually. After freeing all
// associated memory, the function resets the fields of the 
// FunctionCallStatement structure to zero to prevent dangling pointers and
// ensure that the structure is in a clean state.
int parse_function_call_statement(Parser *parser, Statement *statement)
{
    /* Parse a standalone function call and its comma-separated deferred arguments. */
    FunctionCallStatement call;
    memset(&call, 0, sizeof(call));
    Token *name = advance(parser);
    // Check if the current token is a valid function name (identifier) 
    if (name == NULL || peek(parser) == NULL || peek(parser)->type != TOKEN_LEFT_PAREN) return 0;
    call.name = drift_duplicate_string(name->value);
    advance(parser);
    // Collect the arguments for the function call until a right parenthesis is encountered.
    while (peek(parser) != NULL && peek(parser)->type != TOKEN_RIGHT_PAREN) {
        char *argument = collect_expression(parser, TOKEN_COMMA);
        char **args = (char **)realloc(call.arguments, (call.argument_count + 1U) * sizeof(char *));
        if (args == NULL || argument == NULL) {
            free(argument);
            free(args);
            function_call_statement_free(&call);
            return 0;
        }
        call.arguments = args;
        call.arguments[call.argument_count++] = argument;
        if (peek(parser) != NULL && peek(parser)->type == TOKEN_COMMA) advance(parser);
    }
    if (peek(parser) == NULL || peek(parser)->type != TOKEN_RIGHT_PAREN) {
        function_call_statement_free(&call);
        return 0;
    }
    advance(parser);
    if (peek(parser) != NULL && peek(parser)->type == TOKEN_NEWLINE) advance(parser);
    statement->type = STATEMENT_FUNCTION_CALL;
    statement->as.function_call_statement = call;
    return 1;
}

int parse_function_statement(Parser *parser, Statement *statement)
{
    /* Parse `fun name(args...):`, then collect statements through its `end`. */
    FunctionStatement function;
    memset(&function, 0, sizeof(function));
    size_t indentation;
    size_t capacity = 0;
    Token *token;
    advance(parser);
    token = advance(parser);
    if (token == NULL || token->type != TOKEN_IDENTIFIER) return 0;
    function.name = drift_duplicate_string(token->value);
    if (peek(parser) == NULL || peek(parser)->type != TOKEN_LEFT_PAREN) return 0;
    advance(parser);
    while ((token = peek(parser)) != NULL && token->type != TOKEN_RIGHT_PAREN) {
        if (token->type != TOKEN_IDENTIFIER) return 0;
        char **parameters = (char **)realloc(function.parameters, (function.parameter_count + 1U) * sizeof(char *));
        if (parameters == NULL) return 0;
        function.parameters = parameters;
        function.parameters[function.parameter_count++] = drift_duplicate_string(token->value);
        advance(parser);
        if (peek(parser) != NULL && peek(parser)->type == TOKEN_COMMA) advance(parser);
    }
    if (peek(parser) == NULL || peek(parser)->type != TOKEN_RIGHT_PAREN) return 0;
    advance(parser);
    if (peek(parser) == NULL || peek(parser)->type != TOKEN_COLON) return 0;
    advance(parser);
    indentation = token->indentation;
    while ((token = peek(parser)) != NULL && token->type != TOKEN_EOF && token->type != TOKEN_END) {
        if (token->type == TOKEN_NEWLINE) {
            advance(parser);
            continue;
        }
        if (token->indentation <= indentation) break;
        append_statement(&function.body, &function.body_count, &capacity, parser_parse(parser));
    }
    if (peek(parser) != NULL && peek(parser)->type == TOKEN_END) advance(parser);
    statement->type = STATEMENT_FUNCTION;
    statement->as.function_statement = function;
    return 1;
}

void function_statement_free(FunctionStatement *statement)
{
    /* Release function metadata and recursively free its parsed body. */
    if (statement == NULL) return;
    free(statement->name);
    for (size_t i = 0; i < statement->parameter_count; ++i) free(statement->parameters[i]);
    free(statement->parameters);
    for (size_t i = 0; i < statement->body_count; ++i) {
        if (statement->body[i].type == STATEMENT_FUNCTION) function_statement_free(&statement->body[i].as.function_statement);
        else if (statement->body[i].type == STATEMENT_FUNCTION_CALL) function_call_statement_free(&statement->body[i].as.function_call_statement);
        else if (statement->body[i].type == STATEMENT_RETURN) return_statement_free(&statement->body[i].as.return_statement);
    }
    free(statement->body);
    memset(statement, 0, sizeof(*statement));
}

void function_call_statement_free(FunctionCallStatement *statement)
{
    /* Release the call name and every deferred argument string. */
    if (statement == NULL) return;
    free(statement->name);
    for (size_t i = 0; i < statement->argument_count; ++i) free(statement->arguments[i]);
    free(statement->arguments);
    memset(statement, 0, sizeof(*statement));
}

void return_statement_free(ReturnStatement *statement)
{
    /* Release the deferred return expression. */
    if (statement == NULL) return;
    free(statement->expression_text);
    statement->expression_text = NULL;
}
