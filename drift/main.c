/* SPDX-License-Identifier: AGPL-3.0-only */
/* The entry point loads source, lexes and parses it, 
then hands the completed program to the interpreter.  There is also a 
REPL mode that accumulates lines until a complete block is ready 
for execution.  The REPL also recognizes block comments and ignores them. 
But how do you know when a block is complete?  The REPL uses 
indentation and trailing colons to determine whether the user has 
finished typing a block.  If the user types a line that ends with a colon,
the REPL will prompt for more input until the block is closed with an 
`end` statement.  If the user types a line that is indented more than 
the previous line, the REPL will also prompt for more input until the
indentation returns to the previous level.  The REPL also recognizes
the `exit` and `quit` commands to terminate the session.  The REPL is
intended for interactive use, and is not suitable for production use.  
The REPL is also not suitable for use in scripts, as it does not support
piping input or output.  The REPL is intended for use in a terminal, and
does not support GUI applications.  The REPL is also not suitable for
use in web applications, as it does not support web sockets or HTTP requests.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/comments.h"
#include "drift/lexer.h"
#include "drift/parser.h"
#include "drift/interpreter.h"
#include "drift/executable_comments.h"

/*
SO now here we read the source file into a string, then we pass it to the
lexer, which produces a list of tokens.  
The parser then takes the list of tokens and produces a list of 
statements.  The interpreter then takes the list of statements and executes 
them.  The REPL mode accumulates lines until a complete block is ready for
execution, then it passes the accumulated lines to the lexer, parser, and interpreter.
*/
static char *read_file_to_string(const char *path)
{
    FILE *file = fopen(path, "rb");
    char *buffer;
    long length;
    size_t read_size;

    if (file == NULL) {
        fprintf(stderr, "Error: unable to open file '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to seek file '%s'\n", path);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to determine file size for '%s'\n", path);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "Error: unable to rewind file '%s'\n", path);
        return NULL;
    }

    buffer = (char *)malloc((size_t)length + 1U);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "Error: out of memory while reading '%s'\n", path);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)length, file);
    fclose(file);

    if (read_size != (size_t)length) {
        free(buffer);
        fprintf(stderr, "Error: unable to read file '%s'\n", path);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}


// Just a helper function to read the condition text for a 
//while statement. It reads tokens until it finds a colon or a newline, 
//and returns the concatenated text of those tokens. It also trims 
//whitespace from the resulting string.
static char *trim_whitespace(const char *text)
{
    const char *start = text;
    const char *end;
    char *result;
    size_t len;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        result = (char *)malloc(1);
        if (result) result[0] = '\0';
        return result;
    }

    end = start + strlen(start) - 1;
    while (end >= start && isspace((unsigned char)*end)) {
        end--;
    }

    len = end - start + 1;
    result = (char *)malloc(len + 1);
    if (result) {
        memcpy(result, start, len);
        result[len] = '\0';
    }
    return result;
}

/* Counts indentation so interactive input can recognize nested block 
structure. */
static size_t get_leading_spaces(const char *line)
{
    size_t count = 0;
    if (line == NULL) return 0;
    while (line[count] != '\0' && (line[count] == ' ' || line[count] == '\t')) {
        count++;
    }
    return count;
}

/* Detects a trailing block colon while ignoring text that is inside quoted values. */
static int has_pending_block_colon(const char *source)
{
    char *trimmed = trim_whitespace(source);
    int result = 0;

    if (trimmed == NULL || trimmed[0] == '\0') {
        free(trimmed);
        return 0;
    }

    if (trimmed[strlen(trimmed) - 1U] == ':') {
        result = 1;
    }

    free(trimmed);
    return result;
}

/* Combines indentation and delimiters to decide whether REPL input needs more lines. */
static int is_incomplete_block(const char *source)
{
    if (source == NULL || source[0] == '\0') {
        return 0;
    }

    /* Check for trailing colon (pending block opener) */
    if (has_pending_block_colon(source)) {
        return 1;
    }

    return 0;
}

/* Releases the active statement variant and its nested parser-owned 
allocations. Each statement type has its own free function, which is called 
here based on the statement type.  The union in the Statement struct
allows us to store different types of statements in the same memory space,
but we need to know which type it is in order to free it correctly.

*/
static void statement_free(Statement *statement)
{
    if (statement == NULL) {
        return;
    }

    if (statement->type == STATEMENT_PRINT) {
        print_statement_free(&statement->as.print_statement);
    } else if (statement->type == STATEMENT_VARIABLE_DECLARATION) {
        variable_declaration_free(&statement->as.variable_declaration);
    } else if (statement->type == STATEMENT_INPUT) {
        if (statement->as.input_statement.items != NULL) {
            for (size_t i = 0; i < statement->as.input_statement.count; ++i) {
                free(statement->as.input_statement.items[i].prompt);
                free(statement->as.input_statement.items[i].target_name);
            }
            free(statement->as.input_statement.items);
            statement->as.input_statement.items = NULL;
            statement->as.input_statement.count = 0;
        }
    } else if (statement->type == STATEMENT_IF) {
        if_statement_free(&statement->as.if_statement);
    } else if (statement->type == STATEMENT_REPEAT) {
        repeat_statement_free(&statement->as.repeat_statement);
    } else if (statement->type == STATEMENT_FOR) {
        for_statement_free(&statement->as.for_statement);
    } else if (statement->type == STATEMENT_WHILE) {
        while_statement_free(&statement->as.while_statement);
    } else if (statement->type == STATEMENT_EACH) {
        each_statement_free(&statement->as.each_statement);
    } else if (statement->type == STATEMENT_UNLESS) {
        unless_statement_free(&statement->as.unless_statement);
    } else if (statement->type == STATEMENT_WHEN) {
        when_statement_free(&statement->as.when_statement);
    } else if (statement->type == STATEMENT_FUNCTION) {
        function_statement_free(&statement->as.function_statement);
    } else if (statement->type == STATEMENT_FUNCTION_CALL) {
        function_call_statement_free(&statement->as.function_call_statement);
    } else if (statement->type == STATEMENT_RETURN) {
        return_statement_free(&statement->as.return_statement);
    }
}

/* Tokenizes, parses, and executes one complete source buffer in the 
given environment. */
// And ya, this function is a bit long, but it does a lot of work.  
// It takes the source code, lexes it into tokens, parses those tokens into
// statements, and then executes those statements in the given environment.
static int execute_source(const char *source, Environment *environment)
{
    Lexer lexer;
    Token *tokens;
    size_t token_count = 0;
    Parser parser;
    Statement *statements = NULL;
    size_t statement_count = 0;
    size_t statement_capacity = 0;
    int result = 0;
    char *processed_source = NULL;

    // The executable comments are extracted from the source code before 
    //execution.  This allows the user to include comments in their code that
    processed_source = extract_executable_from_exc_blocks(source);
    if (processed_source == NULL) {
        return 1;
    }

    // The lexer scans the source code and produces a list of tokens.
    lexer = lexer_create(processed_source);
    tokens = lexer_scan_all(&lexer, &token_count);
    free(processed_source);
    if (tokens == NULL) {
        return 1;
    }

    // The parser takes the list of tokens and produces a list of 
    // statements.  Each statement is a different type of operation that can
    // be executed by the interpreter.
    parser = parser_create(tokens, token_count);
    while (parser.index < parser.count) {
        Token *tok = &parser.tokens[parser.index];
        if (tok->type == TOKEN_EOF) {
            break;
        }
        if (tok->type == TOKEN_NEWLINE) {
            parser.index++;
            continue;
        }

        if (statement_count == statement_capacity) {
            size_t next_capacity = statement_capacity == 0U ? 8U : statement_capacity * 2U;
            Statement *next = (Statement *)realloc(statements, next_capacity * sizeof(*next));
            if (next == NULL) {
                free(statements);
                token_free_array(tokens, token_count);
                return 1;
            }
            statements = next;
            statement_capacity = next_capacity;
        }
        statements[statement_count++] = parser_parse(&parser);
    }

    for (size_t i = 0; i < statement_count; ++i) {
        result = interpreter_execute(statements[i], environment);
        if (result == DRIFT_EXECUTION_BREAK || result == DRIFT_EXECUTION_CONTINUE) {
            fprintf(stderr, "Runtime Error: Loop control statement used outside a loop.\n");
            result = DRIFT_EXECUTION_ERROR;
        }
        if (result != 0) {
            break;
        }
    }

    for (size_t i = 0; i < statement_count; ++i) {
        statement_free(&statements[i]);
    }
    free(statements);
    token_free_array(tokens, token_count); // Free the array of tokens to avoid memory leaks.

    return result;
}

/* Verifies that a file path uses the Drift source extension 
before execution. File paths that do not end with .df are rejected.   
*/
static int has_df_extension(const char *path)
{
    const char *dot;

    if (path == NULL) {
        return 0;
    }

    dot = strrchr(path, '.');
    return dot != NULL && strcmp(dot, ".df") == 0;
}

/* Reads one source file, executes it, and closes the file
 on every path.
 The file is read into a string, which is then passed to the lexer, 
parser, and interpreter.  The environment is created and freed for 
each file execution.
 */
static int execute_file(const char *path)
{
    char *source;
    Environment environment = environment_create();
    int result = 0;

    if (!has_df_extension(path)) {
        fprintf(stderr, "Error: only .df files can be executed. '%s' is not a valid Drift file.\n", path);
        environment_free(&environment);
        return 1;
    }

    source = read_file_to_string(path);
    if (source == NULL) {
        environment_free(&environment);
        return 1;
    }

    result = execute_source(source, &environment);

    free(source);
    environment_free(&environment);
    return result;
}

/* Runs the interactive loop, accumulating lines until a 
complete block is ready. */
static void run_repl(void)
{
    char line_buffer[4096];
    char *input_buffer = NULL;
    size_t input_length = 0;
    Environment environment = environment_create();

    printf(">>> ");
    fflush(stdout);

    // The REPL reads lines from standard input, accumulating them into 
    // a buffer until a complete block is ready for execution.  
    // The REPL recognizes block comments and ignores them.  The REPL 
    // also recognizes the `exit` and `quit` commands to terminate the session.
    while (fgets(line_buffer, sizeof(line_buffer), stdin) != NULL) {
        size_t line_len = strlen(line_buffer);
        char *new_buf = (char *)realloc(input_buffer, input_length + line_len + 1);
        if (new_buf == NULL) {
            fprintf(stderr, "Error: out of memory in REPL\n");
            free(input_buffer);
            environment_free(&environment);
            return;
        }
        input_buffer = new_buf;
        if (input_length == 0) {
            strcpy(input_buffer, line_buffer);
        } else {
            strcat(input_buffer, line_buffer);
        }
        input_length += line_len;

        char *trimmed = trim_whitespace(input_buffer);
        if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
            free(trimmed);
            break;
        }
        free(trimmed);

        // If the input buffer is empty or only contains whitespace, prompt for more input.
        if (is_block_comment_open(input_buffer) || is_incomplete_block(input_buffer)) {
            printf("... ");
            fflush(stdout);
            continue;
        }

        // If we reach here, we have a complete block of code to execute.
        char *trimmed2 = trim_whitespace(input_buffer);
        if (trimmed2[0] != '\0') {
            execute_source(input_buffer, &environment);
        }
        free(trimmed2);

        free(input_buffer);
        input_buffer = NULL;
        input_length = 0;

        printf(">>> ");
        fflush(stdout);
    }

    free(input_buffer);
    environment_free(&environment);
}

/* Selects file execution or the interactive prompt 
from the command-line arguments. */
int main(int argc, char **argv)
{
    if (argc > 1) {
        return execute_file(argv[1]);
    }

    run_repl();
    return 0;
}
