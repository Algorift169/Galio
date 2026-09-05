/* SPDX-License-Identifier: AGPL-3.0-only */
/* Input parsing turns one line or several fields into typed values used by assignments. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/input.h"
#include "drift/value.h"

/*
 * Trims whitespace from the beginning and end of a string in place.
 * Trimming is done by moving the start pointer forward to skip leading whitespace, 
 * and then moving the end pointer backward to skip trailing whitespace. The string is
 * modified in place, and the function does not return a new string. If the input string
 * is NULL, the function does nothing. The function uses the isspace function to identify whitespace 
 * characters, which includes spaces, tabs, and newlines. The function ensures that the string is
 * null-terminated after trimming, so that it can be used safely in subsequent operations.
 */
static void trim_in_place(char *text)
{
    char *start;  // Pointer to the first non-whitespace character
    char *end;    // Pointer to the last non-whitespace character

    if (text == NULL) {
        return;
    }

    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) { // Skip leading whitespace
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1U);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0'; // Null-terminate the string after trimming trailing whitespace
}

// Reads a line of input from stdin, optionally displaying a prompt. 
// The function uses fgets to read a line of input into a buffer, and then 
// allocates memory for a new string to hold the input line. If a prompt is provided, it is
// displayed before reading the input. The function removes the newline character from the end of
// the input line, if present, and returns a pointer to the newly allocated string. If an error occurs
// during memory allocation or input reading, the function returns NULL. The caller is responsible
// for freeing the allocated memory when it is no longer needed.
char *drift_read_line_from_stdin(const char *prompt)
{
    char buffer[4096]; // Buffer to hold the input line
    char *line = NULL;
    size_t length;

    if (prompt != NULL && prompt[0] != '\0') {
        printf("%s", prompt);
        fflush(stdout); // Ensure the prompt is displayed before reading input 
    }

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    line = (char *)malloc(length + 1U); // Allocate memory for the line to return
    if (line == NULL) {
        fprintf(stderr, "Error: out of memory while reading input\n");
        return NULL;
    }

    strcpy(line, buffer);
    return line;
}

static char **s_input_tokens = NULL;
static size_t s_input_count = 0;
static size_t s_input_index = 0;

static void clear_input_buffer(void) // Clears the input buffer and frees allocated memory
{
    if (s_input_tokens != NULL) {
        for (size_t i = 0; i < s_input_count; ++i) {
            free(s_input_tokens[i]);
        }
        free(s_input_tokens);
        s_input_tokens = NULL;
    }
    s_input_count = 0;
    s_input_index = 0;
}


/*
How to use the input buffer? 
The input buffer is used to store tokens parsed from a line of input. It allows for sequential access 
to the tokens, and it can be cleared when no longer needed. The buffer is initialized by calling
the tokenize_input_line function, which splits a line of input into tokens based on whitespace and 
quotes. The tokens are stored in the s_input_tokens array, and the count of tokens is stored in s_input_count.
*/
static char **tokenize_input_line(const char *line, size_t *out_count)
{
    char **tokens = NULL; // Array of token strings
    size_t capacity = 8;
    size_t count = 0;
    size_t i = 0;
    size_t len = line ? strlen(line) : 0;

    if (out_count != NULL) {
        *out_count = 0;
    }

    if (line == NULL || len == 0) {
        return NULL;
    }

    tokens = (char **)malloc(capacity * sizeof(char *));
    if (tokens == NULL) {
        return NULL;
    }

    // Tokenization loop: iterate through the input line and extract tokens based on whitespace and quotes

    while (i < len) {
        while (i < len && isspace((unsigned char)line[i])) {
            i++;
        }
        if (i >= len) {
            break;
        }

        size_t start = i;
        char quote = '\0';
        if (line[i] == '"' || line[i] == '\'') { // Handle quoted tokens
            quote = line[i];
            i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) {
                    i++;
                }
                i++;
            }
            if (i < len && line[i] == quote) {
                i++;
            }
        } else {
            while (i < len && !isspace((unsigned char)line[i])) {
                i++;
            }
        }

        // Extract the token and add it to the tokens array
        // Allocate memory for the token string and copy it from the input line
        size_t token_len = i - start;
        char *token = (char *)malloc(token_len + 1U);
        if (token != NULL) {
            memcpy(token, line + start, token_len);
            token[token_len] = '\0'; // Null-terminate the token string

            if (count >= capacity) {
                capacity *= 2;
                char **new_tokens = (char **)realloc(tokens, capacity * sizeof(char *));
                if (new_tokens == NULL) {
                    free(token);
                    break;
                }
                tokens = new_tokens;
            }
            tokens[count++] = token; // Add the token to the tokens array
        }
    }

    if (out_count != NULL) {
        *out_count = count;
    }
    return tokens;
}


/*
 * Parses a string representation of an input value and creates a corresponding Value object.
 * The function handles different types of values, including strings, booleans, null, infinity, 
 * integers, and floats. 
 */
int drift_parse_input_value(const char *text, Value *out_value)
{
    char *trimmed;
    char *end = NULL;
    long integer_value;
    double float_value;

    if (text == NULL || out_value == NULL) {
        return 0;
    }

    trimmed = (char *)malloc(strlen(text) + 1U);
    if (trimmed == NULL) {
        return 0;
    }
    strcpy(trimmed, text); // Copy the input text to a mutable string for trimming
    trim_in_place(trimmed); // Trim whitespace from the input string
 
    if ((trimmed[0] == '"' || trimmed[0] == '\'') && strlen(trimmed) >= 2 && trimmed[strlen(trimmed) - 1] == trimmed[0]) {
        size_t len = strlen(trimmed);
        trimmed[len - 1] = '\0';
        *out_value = value_create_string(trimmed + 1);
        free(trimmed);
        return 1;
    }

    // Check for boolean, null, infinity, integer, and float values
    if (strcmp(trimmed, "true") == 0 || strcmp(trimmed, "TRUE") == 0) {
        free(trimmed);
        *out_value = value_create_boolean(1);
        return 1;
    }

    // Check for boolean false value
    if (strcmp(trimmed, "false") == 0 || strcmp(trimmed, "FALSE") == 0) {
        free(trimmed);
        *out_value = value_create_boolean(0);
        return 1;
    }


    // Check for null value
    if (strcmp(trimmed, "null") == 0 || strcmp(trimmed, "NULL") == 0) {
        free(trimmed);
        *out_value = value_create_null();
        return 1;
    }

    // 
    if (strcmp(trimmed, "inf") == 0 || strcmp(trimmed, "INF") == 0) {
        free(trimmed);
        *out_value = value_create_infinity();
        return 1;
    }

    // Attempt to parse the trimmed string as an integer
    integer_value = strtol(trimmed, &end, 10);
    if (end != trimmed && (*end == '\0' || isspace((unsigned char)*end))) {
        free(trimmed);
        *out_value = value_create_integer(integer_value);
        return 1;
    }

    // Attempt to parse the trimmed string as a float
    float_value = strtod(trimmed, &end);
    if (end != trimmed && (*end == '\0' || isspace((unsigned char)*end))) {
        free(trimmed);
        *out_value = value_create_float(float_value);
        return 1;
    }

    free(trimmed); // Free the trimmed string if no other value type matched
    *out_value = value_create_string(text); // 
    return 1;
}


/*
 * Prompts the user for input and stores the parsed value in the specified location.
 */
int drift_prompt_and_store(const char *prompt, const char *target_name, Value *out_value)
{
    if (target_name == NULL || out_value == NULL) {
        return 0;
    }

    if (prompt != NULL && prompt[0] != '\0') {
        clear_input_buffer();
    }

    if (s_input_tokens != NULL && s_input_index < s_input_count) {
        char *token = s_input_tokens[s_input_index++]; // Check if there are remaining tokens in the input buffer

        int ok = drift_parse_input_value(token, out_value);
        if (s_input_index >= s_input_count) {
            clear_input_buffer();
        }
        return ok;
    }

    clear_input_buffer();

    // Prompt the user for input and read a line from stdin, then tokenize and parse the input value
    // The function enters a loop where it repeatedly prompts the user for input until a 
    // valid value is provided. It uses the drift_read_line_from_stdin function to read a line of 
    // input from stdin, and then tokenizes the input line using the tokenize_input_line function.
    const char *current_prompt = prompt;
    while (1) {
        char *line = drift_read_line_from_stdin(current_prompt);
        if (line == NULL) {
            *out_value = value_create_string(""); 
            return 1;
        }

        size_t count = 0;
        char **tokens = tokenize_input_line(line, &count);
        free(line);

        if (tokens != NULL && count > 0) {
            s_input_tokens = tokens;
            s_input_count = count;
            s_input_index = 0;

            // Parse the first token as the input value and store it in out_value
            char *token = s_input_tokens[s_input_index++];
            int ok = drift_parse_input_value(token, out_value);
            if (s_input_index >= s_input_count) {
                clear_input_buffer();
            }
            return ok;
        }

        // If no tokens were found, prompt the user again for input
        if (tokens != NULL) {
            free(tokens);
        }
        current_prompt = NULL;
    }
}
