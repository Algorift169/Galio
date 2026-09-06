/* SPDX-License-Identifier: AGPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drift/platform.h"

char *drift_platform_read_line(const char *prompt)
{
    char buffer[4096];
    size_t length;
    char *line;

    if (prompt != NULL && prompt[0] != '\0') {
        printf("%s", prompt);
        fflush(stdout);
    }
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return NULL;
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') buffer[--length] = '\0';
    line = (char *)malloc(length + 1U);
    if (line == NULL) return NULL;
    memcpy(line, buffer, length + 1U);
    return line;
}
