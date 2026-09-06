/* SPDX-License-Identifier: AGPL-3.0-only */
#include <stdlib.h>

#include "drift/executable_comments.h"
#include "drift/interpreter.h"
#include "drift/lexer.h"
#include "drift/parser.h"
#include "drift/runtime.h"

static void runtime_statement_free(Statement *statement)
{
    if (statement == NULL) return;
    switch (statement->type) {
        case STATEMENT_PRINT: print_statement_free(&statement->as.print_statement); break;
        case STATEMENT_VARIABLE_DECLARATION: variable_declaration_free(&statement->as.variable_declaration); break;
        case STATEMENT_IF: if_statement_free(&statement->as.if_statement); break;
        case STATEMENT_REPEAT: repeat_statement_free(&statement->as.repeat_statement); break;
        case STATEMENT_FOR: for_statement_free(&statement->as.for_statement); break;
        case STATEMENT_WHILE: while_statement_free(&statement->as.while_statement); break;
        case STATEMENT_EACH: each_statement_free(&statement->as.each_statement); break;
        case STATEMENT_UNLESS: unless_statement_free(&statement->as.unless_statement); break;
        case STATEMENT_WHEN: when_statement_free(&statement->as.when_statement); break;
        case STATEMENT_FUNCTION: function_statement_free(&statement->as.function_statement); break;
        case STATEMENT_FUNCTION_CALL: function_call_statement_free(&statement->as.function_call_statement); break;
        case STATEMENT_RETURN: return_statement_free(&statement->as.return_statement); break;
        case STATEMENT_COMMAND: command_statement_free(&statement->as.command_statement); break;
        default: break;
    }
}

int drift_execute_source(const char *source, Environment *environment)
{
    char *processed_source;
    Lexer lexer;
    Token *tokens;
    size_t token_count = 0;
    Parser parser;
    Statement *statements = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int result = 0;

    if (source == NULL || environment == NULL) return 1;
    processed_source = extract_executable_from_exc_blocks(source);
    if (processed_source == NULL) return 1;
    lexer = lexer_create(processed_source);
    tokens = lexer_scan_all(&lexer, &token_count);
    free(processed_source);
    if (tokens == NULL) return 1;

    parser = parser_create(tokens, token_count);
    while (parser.index < parser.count && parser.tokens[parser.index].type != TOKEN_EOF) {
        if (parser.tokens[parser.index].type == TOKEN_NEWLINE) {
            parser.index++;
            continue;
        }
        if (count >= capacity) {
            size_t next_capacity = capacity == 0 ? 8U : capacity * 2U;
            Statement *next = (Statement *)realloc(statements, next_capacity * sizeof(*next));
            if (next == NULL) { result = 1; break; }
            statements = next;
            capacity = next_capacity;
        }
        statements[count++] = parser_parse(&parser);
    }

    if (result == 0) {
        for (size_t i = 0; i < count; ++i) {
            result = interpreter_execute(statements[i], environment);
            if (result != DRIFT_EXECUTION_OK) break;
        }
    }
    for (size_t i = 0; i < count; ++i) runtime_statement_free(&statements[i]);
    free(statements);
    token_free_array(tokens, token_count);
    return result;
}
