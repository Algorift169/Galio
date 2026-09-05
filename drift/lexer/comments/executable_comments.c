/* SPDX-License-Identifier: AGPL-3.0-only */
/* Executable comments are filtered into source text so embedded 
code follows ordinary lexer ordering. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "drift/executable_comments.h"

/* Helper to check if a trimmed line matches @exc or .exc */
/* Checks whether a comment line contains the requested executable marker. */
static int is_exc_marker(const char *line, const char *marker)
{
    if (line == NULL || marker == NULL) {
        return 0;
    }
    return strcmp(line, marker) == 0;
}

/* Trim leading/trailing whitespace from a line
   and return a newly allocated string. Caller must free the result.
   But if the line is empty or NULL, return an empty string. 
*/
static char *trim_line(const char *line, size_t line_len)
{
    const char *start = line;
    const char *end = line + line_len;

    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }

    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t')) {
        end--;
    }

    size_t trimmed_len = end - start;
    char *result = (char *)malloc(trimmed_len + 1);
    if (result) {
        memcpy(result, start, trimmed_len);
        result[trimmed_len] = '\0';
    }
    return result;
}


// Strips line comments from a line of code, preserving quoted 
// strings. Returns a newly allocated string, or NULL on error. Caller
// must free the result. If the line is empty or NULL, returns an 
// empty string.
static char *strip_line_comment(const char *line)
{
    size_t i = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    char *result;

    if (line == NULL) {
        return NULL;
    }

    while (line[i] != '\0') {
        char c = line[i];
        if (c == '\\' && in_double_quote && line[i + 1] != '\0') {
            i += 2;
            continue;
        }
        if (c == '\\' && in_single_quote && line[i + 1] != '\0') {
            i += 2;
            continue;
        }
        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            i++;
            continue;
        }
        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            i++;
            continue;
        }
        if (c == '/' && line[i + 1] == '/' && !in_single_quote && !in_double_quote) {
            break;
        }
        i++;
    }

    result = trim_line(line, i);
    return result;
}

/* Check if current position starts a block comment closing sequence */
/* Scans forward from a block-comment position to locate its closing delimiter. */
static int check_block_end(const char *source, size_t pos, size_t len)
{
    if (pos + 1 < len && source[pos] == '*' && source[pos + 1] == '/') {
        return 1;
    }
    return 0;
}


// There are two types of executable comments: @exc and .exc. 
// @exc marks the start of an executable comment block, while .exc 
// marks the end of an executable comment block. The function below extracts
// the executable code from the source text, ignoring any non-executable
// comments and whitespace. It returns a newly allocated string containing
// the extracted code, or NULL on error. The caller is responsible for 
// freeing the returned string.
char *extract_executable_from_exc_blocks(const char *source)
{
    size_t source_len;
    char *result;
    size_t result_idx = 0;
    size_t i = 0;
    int in_block_comment = 0;
    int in_exc_block = 0;

    if (source == NULL) {
        result = (char *)malloc(1);
        if (result) result[0] = '\0';
        return result;
    }

    source_len = strlen(source);
    result = (char *)malloc(source_len + 1);
    if (result == NULL) {
        return NULL;
    }

    while (i < source_len) {
        if (!in_block_comment && i + 1 < source_len && source[i] == '/' && source[i + 1] == '*') {
            in_block_comment = 1;
            i += 2;
            continue;
        }

        if (!in_block_comment) {
            result[result_idx++] = source[i++];
            continue;
        }

        size_t line_start = i;
        size_t line_end = i;

        while (line_end < source_len && source[line_end] != '\n') {
            if (check_block_end(source, line_end, source_len)) {
                break;
            }
            line_end++;
        }

        // If the block comment ends on the same line, 
        // we need to check if the line contains an executable comment 
        // If the block comment ends on the same line, we need to 
        // check if the line contains an executable comment marker. If
        // it does, we extract the code from that line. Otherwise, we skip 
        // the line and continue to the next one.
        if (line_end < source_len && source[line_end] != '\n') {
            char *trimmed = trim_line(source + line_start, line_end - line_start);
            if (trimmed) {
                if (is_exc_marker(trimmed, ".exc")) {
                    in_exc_block = 0;
                }
                free(trimmed);
            }
            in_block_comment = 0;
            i = line_end + 2;
            continue;
        }

        // If the block comment spans multiple lines, we need to check
        // each line for executable comment markers. If we find an 
        // @exc marker, we set the in_exc_block flag to true. If we find 
        // a .exc marker, we set the in_exc_block flag to false. If we are
        // in an executable comment block, we extract the code from that 
        // line and append it to the result. Otherwise, we skip the line.
        char *trimmed = trim_line(source + line_start, line_end - line_start);

        if (trimmed) {
            char *code_line = NULL;
            if (is_exc_marker(trimmed, "@exc")) {
                in_exc_block = 1;
            } else if (is_exc_marker(trimmed, ".exc")) {
                in_exc_block = 0;
            } else if (in_exc_block && trimmed[0] != '\0') {
                code_line = strip_line_comment(trimmed);
                if (code_line != NULL && code_line[0] != '\0') {
                    if (result_idx > 0 && result[result_idx - 1] != '\n') {
                        result[result_idx++] = '\n';
                    }
                    strcpy(&result[result_idx], code_line);
                    result_idx += strlen(code_line);
                    result[result_idx++] = '\n';
                }
                free(code_line);
            }
            free(trimmed);
        }

        i = line_end;
        if (i < source_len && source[i] == '\n') {
            i++;
        }
    }

    result[result_idx] = '\0';
    return result;
}
